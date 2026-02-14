# Smart Breath Analyzer

ระบบวิเคราะห์ลมหายใจอัจฉริยะสำหรับตรวจวัดและวิเคราะห์คุณภาพอากาศในลมหายใจ

## โครงสร้างระบบ

### 1. Authentication Flow (ระบบ Login/Register)

```
login.html → intro.html → dashboard (index.html) → page2.html → end.html → intro.html (loop)
```

#### หน้า Login (`login.html`)
- รองรับทั้ง Login และ Register
- ข้อมูลผู้ใช้เก็บใน `users.json` ผ่าน Flask backend
- หลังจาก login สำเร็จจะ redirect ไปยัง `intro.html`

#### หน้า Intro (`intro.html`)
- หน้าแนะนำโปรเจ็กต์
- แสดง username และปุ่ม logout ที่มุมขวาบน
- ปุ่ม "เข้าสู่ระบบเพื่อเริ่มใช้งาน" จะไปยัง Dashboard

#### หน้า Dashboard (`index.html`)
- หน้าหลักสำหรับแสดงข้อมูลเซนเซอร์แบบเรียลไทม์
- แสดงกราฟและค่าต่างๆ จากเซนเซอร์ MQ2, MQ3, MQ7, MQ8, MQ135
- มีปุ่ม "เริ่มเก็บข้อมูล" และ "หยุดการวัด"
- แสดง username และปุ่ม logout ที่มุมขวาบน

#### หน้าวิเคราะห์ผล (`page2.html`)
- แสดงผลการวิเคราะห์และการประเมินความเสี่ยง
- แสดง username และปุ่ม logout ที่มุมขวาบน

#### หน้าสรุป (`end.html`)
- หน้าสรุปและแสดงข้อมูลผู้พัฒนา
- ปุ่ม "เริ่มใหม่" จะกลับไปหน้า intro (ถ้า login อยู่แล้วไม่ต้อง login ใหม่)
- แสดง username และปุ่ม logout ที่มุมขวาบน

### 2. Features

✅ **ระบบ Authentication**
- Login/Register ผ่าน Flask session
- เก็บข้อมูลผู้ใช้ใน `users.json`
- Session persistence ตลอดการใช้งาน

✅ **User Display**
- แสดงชื่อผู้ใช้มุมขวาบนทุกหน้า (ยกเว้นหน้า login)
- ปุ่ม Logout พร้อม confirmation

✅ **Navigation Flow**
- ถ้า login แล้ว: สามารถเข้าถึงทุกหน้าได้
- ถ้ายัง logout: กดปุ่มเริ่มใหม่ที่หน้า end จะกลับไป intro โดยไม่ต้อง login ใหม่

### 3. การติดตั้งและรัน

#### ข้อกำหนดเบื้องต้น
```bash
pip install flask psutil --break-system-packages
```

#### การรันระบบ
```bash
python3 app.py
```

ระบบจะรันที่:
- **URL**: `http://localhost:5022` หรือ `http://0.0.0.0:5022`
- **Default Login**: 
  - Username: `admin`
  - Password: `1234`

### 4. โครงสร้างไฟล์

```
/
├── app.py                 # Flask backend
├── templates/
│   ├── login.html        # หน้า login/register
│   ├── intro.html        # หน้าแนะนำ
│   ├── index.html        # Dashboard
│   ├── page2.html        # หน้าวิเคราะห์ผล
│   ├── end.html          # หน้าสรุป
│   └── user.html         # หน้าข้อมูลผู้ใช้ (optional)
└── users.json            # ฐานข้อมูลผู้ใช้ (สร้างอัตโนมัติ)
```

### 5. API Endpoints

#### Authentication
- `GET /` - Redirect to login
- `GET/POST /login` - Login/Register page
- `GET /logout` - Logout and clear session

#### Main Pages
- `GET /intro` - หน้าแนะนำ (ต้อง login)
- `GET /dashboard` - Dashboard (ต้อง login)
- `GET /page2` - หน้าวิเคราะห์ (ต้อง login)
- `GET /end` - หน้าสรุป (ต้อง login)
- `GET /user` - หน้าข้อมูลผู้ใช้ (ต้อง login)

#### API for ESP32/Sensors
- `GET/POST /api/status` - รับ/ส่งข้อมูลเซนเซอร์
- `POST /api/start_test` - เริ่มการวัด
- `POST /api/stop_test` - หยุดการวัด

### 6. การทำงานของระบบ

1. **เริ่มต้น**: ผู้ใช้เข้า `/` → redirect ไป `/login`
2. **Login**: กรอก username/password → บันทึก session → redirect ไป `/intro`
3. **Intro**: แสดงข้อมูลโปรเจ็กต์ → กด "เข้าสู่ระบบเพื่อเริ่มใช้งาน" → ไป `/dashboard`
4. **Dashboard**: ตรวจวัดข้อมูล → กด "ดูผลการวิเคราะห์" → ไป `/page2`
5. **Page2**: วิเคราะห์ผล → กด "ดูสรุป" → ไป `/end`
6. **End**: กด "เริ่มใหม่" → กลับไป `/intro` (ยังคง login อยู่)
7. **Logout**: กดปุ่ม Logout ที่มุมขวาบน → กลับไป `/login`

### 7. การปรับแต่ง

#### เปลี่ยนสี Theme
แก้ไขค่าใน `:root` ของแต่ละไฟล์:
```css
:root {
    --primary: #123580;
    --accent: #5e81ac;
    --bg: #eef2f7;
}
```

#### เพิ่มผู้ใช้ใหม่
สามารถ Register ผ่านหน้า login หรือแก้ไข `users.json` โดยตรง

### 8. ข้อมูลผู้พัฒนา

- **Mr. Podchara Pribwai** - ม.3/1 เลขที่ 5
- **Ms. Wannida Chantarakun** - ม.3/1 เลขที่ 20
- **Ms. Sicha Luechoowong** - ม.3/1 เลขที่ 22

วิชา: คอมพิวเตอร์ 6 (ว23202)

---

## License
© 2026 Smart Breath Analyzer Project
