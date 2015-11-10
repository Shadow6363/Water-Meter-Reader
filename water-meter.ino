
#define db_host ""
#define db_port 0000
#define db_user ""
#define db_pass ""

#define hmc5883l_address 0x1E
#define publish_delay 10000


STARTUP(WiFi.selectAntenna(ANT_EXTERNAL));


unsigned long last_publish = 0;
unsigned long now = 0;
unsigned int crossings = 0;

int new_val = 0;
int old_val = 0;

boolean changed = false;

TCPClient client;
int16_t y;


void setup() {
    Wire.begin();

    Wire.beginTransmission(hmc5883l_address);
    Wire.write(0x00); // Select Configuration Register A
    Wire.write(0x38); // 2 Averaged Samples at 75Hz
    Wire.endTransmission();

    Wire.beginTransmission(hmc5883l_address);
    Wire.write(0x01); // Select Configuration Register B
    Wire.write(0x20); // Set Default Gain
    Wire.endTransmission();

    Wire.beginTransmission(hmc5883l_address);
    Wire.write(0x02); // Select Mode Register
    Wire.write(0x00); // Continuous Measurement Mode
    Wire.endTransmission();
}

void loop() {
    now = millis();

    Wire.beginTransmission(hmc5883l_address);
    Wire.write(0x03); // Select register 3, X MSB Register
    Wire.endTransmission();

    Wire.requestFrom(hmc5883l_address, 6); delay(4);
    if(Wire.available() >= 6) {
        Wire.read(); Wire.read(); Wire.read(); Wire.read(); // Ignore X and Z Registers
        y  = Wire.read() << 8; // Y MSB
        y |= Wire.read();      // Y LSB
    }

    old_val = new_val;
    new_val = map(y, -928, -328, -300, 300);
    changed = (old_val < 0 && new_val > 0) || (old_val > 0 && new_val < 0);

    if(changed) {
        crossings += 1;
    }

    if(crossings > 0 && (now - last_publish) >= publish_delay) {
        if(client.connected() || client.connect(db_host, db_port)) {
            String influx_data =
                String::format("water value=%u", crossings);
            String http_content_len =
                String::format("Content-Length: %u", strlen(influx_data));
            String http_host =
                String::format("Host: %s:%u", db_host, db_port);
            String http_request =
                String::format("POST /write?db=meters&u=%s&p=%s HTTP/1.1",
                                db_user, db_pass);

            client.println(http_request);
            client.println(http_host);
            client.println(http_content_len);
            client.println();
            client.println(influx_data);
            client.println();
            client.flush();
            client.stop();

            crossings = 0;
            last_publish = now;
        }
    }

    delay(10);
}
