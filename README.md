# ESP32 IoT LED Controller dengan MQTT, FreeRTOS, dan NVS

Project ini merupakan implementasi **manajemen mikroprosesor pada ESP32** yang menggabungkan **interrupt hardware**, **multitasking FreeRTOS**, **manajemen memori non-volatile (NVS)**, **PWM**, serta **komunikasi jaringan (WiFi, MQTT, dan HTTP)**.

Sistem dirancang sebagai **IoT Smart LED Controller** yang dapat dikontrol secara lokal maupun remote dengan monitoring real-time.

---

##  Fitur Utama

*  Interrupt handling tanpa polling (respons cepat & efisien)
*  Penyimpanan konfigurasi permanen menggunakan NVS
*  Multitasking berbasis FreeRTOS
*  Kontrol LED menggunakan PWM (3 level brightness)
*  Komunikasi IoT menggunakan MQTT Secure (TLS)
*  Web server HTTP sebagai dashboard kontrol

---

##  Konsep Manajemen Mikroprosesor yang Diterapkan

### 1. Interrupt Handling (Tanpa Polling)

Sistem menggunakan **hardware interrupt** untuk mendeteksi penekanan tombol tanpa perlu polling terus-menerus.

```cpp
void IRAM_ATTR handleButton(){
  buttonPressed = true;
}

void setup(){
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, handleButton, FALLING);
}
```

🔹 Keunggulan:

* Respons lebih cepat
* Hemat CPU time
* Efisien untuk embedded system

---

### 2. Manajemen Memori dengan NVS (Persistent Storage)

ESP32 menggunakan **Non-Volatile Storage (NVS)** untuk menyimpan konfigurasi sistem di flash memory.

```cpp
Preferences prefs;

void setup(){
  prefs.begin("cfg", false);
  ledPower = prefs.getBool("power", false);
  brightnessLevel = prefs.getInt("level", 0);
}

// Saat ada perubahan
prefs.putBool("power", ledPower);
prefs.putInt("level", brightnessLevel);
```

 Data yang disimpan:

* Status LED (ON/OFF)
* Level brightness (0, 1, 2)

🔹 Manfaat:

* Data tetap tersimpan meskipun ESP32 restart atau mati listrik
* Cocok untuk konfigurasi sistem IoT

---

### 3. Multitasking dengan FreeRTOS

ESP32 menjalankan task paralel menggunakan **FreeRTOS** untuk monitoring otomatis.

```cpp
void monitorTask(void* p){
  for(;;){
    if(millis() - lastPublish > interval){
      lastPublish = millis();
      publishSource = "rtos";
      needPublish = true;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void setup(){
  xTaskCreate(monitorTask, "Monitor", 4096, NULL, 1, NULL);
}
```

 Task ini:

* Berjalan setiap 10 detik
* Mengirim status LED ke MQTT Cloud
* Monitoring real-time tanpa request manual

---

### 4. Kontrol PWM (Pulse Width Modulation)

LED dikontrol menggunakan PWM untuk mengatur intensitas cahaya secara efisien.

```cpp
#define PWM_FREQ 5000
#define PWM_RES  8
int pwmLevels[3] = {5, 80, 255};

void setup(){
  ledcAttach(LED1_PIN, PWM_FREQ, PWM_RES);
}

void applyLed(){
  currentPwm = ledPower ? pwmLevels[brightnessLevel] : 0;
  ledcWrite(LED1_PIN, currentPwm);
}
```

 Level Brightness:

* Level 0 → PWM 5  (~2%)  → Redup
* Level 1 → PWM 80 (~31%) → Sedang
* Level 2 → PWM 255 (100%) → Terang

🔹 PWM lebih hemat energi dibanding ON/OFF penuh

---

### 5. Komunikasi Data (WiFi + MQTT + HTTP)

```cpp
// WiFi Connection
WiFi.begin(ssid, password);

// MQTT Secure (TLS)
WiFiClientSecure secureClient;
secureClient.setInsecure();
mqtt.setServer(mqtt_server, mqtt_port);

// HTTP Web Server
server.on("/cmd", [](){
  mqtt.publish(mqtt_topic_control, buf);
});
```

 Arsitektur Komunikasi:

1. **WiFi**

   * Koneksi ke jaringan lokal

2. **MQTT Secure (TLS)**

   * Port 8883
   * Broker: HiveMQ Cloud
   * Publish status otomatis
   * Subscribe perintah kontrol

3. **HTTP Web Server**

   * Port 80
   * Dashboard berbasis web
   * Kontrol LED: ON / OFF / TOGGLE
   * Real-time update via JavaScript

🔹 Sistem bisa dikontrol **lokal maupun remote**

---

##  Video Demo & Diagram Blok

Video demo sistem dan diagram blok arsitektur dapat diakses melalui link berikut:

- Google Drive (Video & Diagram Blok):  
  https://drive.google.com/drive/folders/10TdPYG_W0BkWQMEL7UgYImUm4TzD98cZ?usp=sharing

- YouTube (Demo Video Versi YouTube):  
  https://youtu.be/2lUDEG6GO9E

---

##  Kesimpulan

Project ini menunjukkan penerapan konsep manajemen mikroprosesor modern pada ESP32, meliputi:

* Interrupt berbasis hardware
* Multitasking real-time dengan FreeRTOS
* Penyimpanan data permanen menggunakan NVS
* Kontrol PWM untuk efisiensi daya
* Arsitektur komunikasi IoT yang lengkap

