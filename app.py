import os
import json
import psutil
from flask import Flask, render_template, jsonify, request, session, redirect, url_for
from datetime import datetime

app = Flask(__name__)
app.secret_key = 'smart_breath_analyzer_key'

USER_DATA_FILE = 'users.json'
HISTORY_DATA_FILE = 'history.json'

# --- ระบบจัดการผู้ใช้งาน ---
def load_users():
    if os.path.exists(USER_DATA_FILE):
        with open(USER_DATA_FILE, 'r', encoding='utf-8') as f:
            return json.load(f)
    return {
        "admin": {
            "password": "1234",
            "firstname": "Admin",
            "lastname": "System",
            "birthdate": "2000-01-01"
        }
    }

def save_users(users):
    with open(USER_DATA_FILE, 'w', encoding='utf-8') as f:
        json.dump(users, f, indent=4, ensure_ascii=False)

def load_history():
    if os.path.exists(HISTORY_DATA_FILE):
        with open(HISTORY_DATA_FILE, 'r', encoding='utf-8') as f:
            return json.load(f)
    return {}

def save_history(history):
    with open(HISTORY_DATA_FILE, 'w', encoding='utf-8') as f:
        json.dump(history, f, indent=4, ensure_ascii=False)

def add_history_record(username, assessment):
    """เพิ่มประวัติการตรวจ"""
    history = load_history()
    
    if username not in history:
        history[username] = []
    
    now = datetime.now()
    record = {
        'date': now.strftime('%d/%m/%Y'),
        'time': now.strftime('%H:%M:%S'),
        'assessment': assessment,
        'timestamp': now.isoformat()
    }
    
    history[username].append(record)
    save_history(history)

def thai_date(date_str):
    """แปลงวันที่เป็นรูปแบบไทย"""
    try:
        date_obj = datetime.strptime(date_str, '%Y-%m-%d')
        thai_months = [
            'มกราคม', 'กุมภาพันธ์', 'มีนาคม', 'เมษายน', 
            'พฤษภาคม', 'มิถุนายน', 'กรกฎาคม', 'สิงหาคม',
            'กันยายน', 'ตุลาคม', 'พฤศจิกายน', 'ธันวาคม'
        ]
        thai_year = date_obj.year + 543
        return f"{date_obj.day} {thai_months[date_obj.month-1]} {thai_year}"
    except:
        return date_str

# --- ตัวแปร Global สำหรับเก็บค่าล่าสุดจาก ESP32 ---
latest_data = {
    'dht_temperature': 0.0,
    'humidity': 0.0,
    'sensors': {
        'mq2_ppm': 0.0,
        'mq3_ppm': 0.0,
        'mq7_ppm': 0.0,
        'mq8_ppm': 0.0,
        'mq135_ppm': 0.0
    }
}

# สถานะบอกบอร์ดว่าหน้าเว็บกำลังกด "เริ่มเก็บข้อมูล" หรือไม่
is_measuring = False

def get_system_stats():
    """ดึงข้อมูลทรัพยากรเครื่อง Server"""
    return {
        'server_temperature': 45.0, # ค่าจำลอง
        'cpu': psutil.cpu_percent(),
        'memory': psutil.virtual_memory().percent,
    }

# --- Routes Logic ---

@app.route('/')
def root():
    return redirect(url_for('login'))

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        # รองรับทั้งแบบ Form และ JSON
        data = request.form if request.form else request.get_json()
        username = data.get('username')
        password = data.get('password')
        action = data.get('action', 'login')
        
        users = load_users()
        
        if action == 'register':
            firstname = data.get('firstname')
            lastname = data.get('lastname')
            birthdate = data.get('birthdate')
            
            if username in users:
                return jsonify({"status": "error", "message": "ชื่อผู้ใช้นี้ถูกใช้งานแล้ว"})
            
            users[username] = {
                "password": password,
                "firstname": firstname,
                "lastname": lastname,
                "birthdate": birthdate
            }
            save_users(users)
            return jsonify({"status": "success"})
            
        elif action == 'login':
            if username in users:
                user_data = users[username]
                # รองรับทั้งรูปแบบเก่า (string) และรูปแบบใหม่ (dict)
                if isinstance(user_data, str):
                    # รูปแบบเก่า: {"username": "password"}
                    if user_data == password:
                        session['username'] = username
                        session.permanent = True
                        return redirect(url_for('intro'))
                else:
                    # รูปแบบใหม่: {"username": {"password": "...", "firstname": "..."}}
                    if user_data.get('password') == password:
                        session['username'] = username
                        session.permanent = True
                        return redirect(url_for('intro'))
            
            return "Invalid credentials <a href='/login'>Try again</a>"
            
    return render_template('login.html')

@app.route('/intro')
def intro():
    if 'username' not in session:
        return redirect(url_for('login'))
    return render_template('intro.html')

@app.route('/index')
def index():
    if 'username' not in session:
        return redirect(url_for('login'))
    return render_template('index.html', stats=get_system_stats(), user=session['username'])

# --- API สำหรับการเชื่อมต่อกับ ESP32 และ index ---

@app.route('/api/status', methods=['GET', 'POST'])
def api_status():
    global latest_data, is_measuring
    
    if request.method == 'POST':
        # รับข้อมูล JSON จาก ESP32
        data = request.get_json()
        if data:
            latest_data['dht_temperature'] = data.get('dht_temperature', 0)
            latest_data['humidity'] = data.get('humidity', 0)
            # รับค่าจากบอร์ดเข้าสู่โครงสร้าง sensors
            if 'sensors' in data:
                latest_data['sensors'] = data['sensors']
        
        # ส่งสถานะ is_measuring กลับไปให้บอร์ดเปลี่ยนหน้าจอ LCD
        return jsonify({"is_measuring": is_measuring})
    
    # ถ้าเป็น GET (หน้าเว็บเรียกมาอัปเดตกราฟ)
    response = get_system_stats()
    response.update(latest_data)
    return jsonify(response)

@app.route('/api/start_test', methods=['POST'])
def start_test():
    global is_measuring
    is_measuring = True
    return jsonify({"status": "measuring"})

@app.route('/api/stop_test', methods=['POST'])
def stop_test():
    global is_measuring
    is_measuring = False
    return jsonify({"status": "idle"})

@app.route('/page2')
def page2():
    if 'username' not in session:
        return redirect(url_for('login'))
    return render_template('page2.html', user=session['username'])

@app.route('/end')
def end():
    if 'username' not in session:
        return redirect(url_for('login'))
    return render_template('end.html', user=session['username'])

@app.route('/user')
def user_profile():
    if 'username' not in session:
        return redirect(url_for('login'))
    
    username = session['username']
    users = load_users()
    user_data = users.get(username, {})
    
    # จัดการข้อมูลผู้ใช้
    if isinstance(user_data, str):
        # รูปแบบเก่า
        user_info = {
            'firstname': username,
            'lastname': '',
            'birthdate': 'ไม่ระบุ',
            'birthdate_thai': 'ไม่ระบุ'
        }
    else:
        # รูปแบบใหม่
        user_info = {
            'firstname': user_data.get('firstname', username),
            'lastname': user_data.get('lastname', ''),
            'birthdate': user_data.get('birthdate', 'ไม่ระบุ'),
            'birthdate_thai': thai_date(user_data.get('birthdate', '')) if user_data.get('birthdate') else 'ไม่ระบุ'
        }
    
    # โหลดประวัติการตรวจ
    all_history = load_history()
    user_history = all_history.get(username, [])
    
    # จัดการ risk class สำหรับแสดงสี
    for record in user_history:
        assessment = record.get('assessment', '')
        if 'Normal' in assessment:
            record['risk_class'] = 'risk-normal'
        elif 'Caution' in assessment:
            record['risk_class'] = 'risk-caution'
        else:
            record['risk_class'] = 'risk-danger'
    
    # เรียงจากใหม่ไปเก่า
    user_history.reverse()
    
    return render_template('user.html', 
                         user_info=user_info, 
                         history=user_history)

@app.route('/api/save_assessment', methods=['POST'])
def save_assessment():
    """บันทึกผลการประเมินความเสี่ยง"""
    if 'username' not in session:
        return jsonify({"status": "error", "message": "Not logged in"}), 401
    
    data = request.get_json()
    assessment = data.get('assessment', 'Status: Normal')
    
    add_history_record(session['username'], assessment)
    
    return jsonify({"status": "success"})

@app.route('/logout')
def logout():
    session.clear()
    return redirect(url_for('login'))

if __name__ == '__main__':
    # รันบน IP 0.0.0.0 เพื่อให้ ESP32 เชื่อมต่อผ่าน WiFi ได้
    app.run(host='0.0.0.0', port=5022, debug=True)