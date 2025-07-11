//--------------------------------------------------------
//  25TA Class  Shooting Game
//
//--------------------------------------------------------
//  Include
//--------------------------------------------------------
#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"
#include <SoftwareSerial.h>
#include <Wire.h>

//--------------------------------------------------------
// Games
#define UNIT               6

#define GAME_TIME_S        60

#define GAME_JUDGE_TIME1   500
#define GAME_JUDGE_TIME2   1000
#define GAME_JUDGE_TIME3   1500
#define GAME_JUDGE_TIME4   2000

#define GAME_POINT1        155
#define GAME_POINT2        135
#define GAME_POINT3        75
#define GAME_POINT4        35


//--------------------------------------------------------
// PIN
#define PIN_LED         13
#define PIN_PSW         12

//--------------------------------------------------------
// DF Player
//--------------------------------------------------------
// https://qiita.com/ajimaru/items/dfd1e75d8de20980f665
//
//--------------------------------------------------------
// DFPlayer Mimi UART Pin Define
#define PIN_DFP_TX      11
#define PIN_DFP_RX      10
#define PIN_DFP_BUSY    9             // Active Low

// Sound Folder
#define DFP_FOLDER_01   01

// Sound Files
#define DFP_FILE_001    1 //game start
#define DFP_FILE_002    2 //game finish
#define DFP_FILE_003    3 //score 1
#define DFP_FILE_004    4 //score 2
#define DFP_FILE_005    5 //score 3
#define DFP_FILE_006    6 //score 4
#define DFP_FILE_007    7 //score5 
#define DFP_FILE_008    8 //combo4
#define DFP_FILE_009    9 //combo5
#define DFP_FILE_010    10 //combo3
#define DFP_FILE_011    11 //combo2
#define DFP_FILE_012    12 //start test
#define DFP_FILE_013    13 //connect
#define DFP_FILE_014    14 //last

SoftwareSerial          DFPSerial(PIN_DFP_RX, PIN_DFP_TX);
DFRobotDFPlayerMini     DFPlayer;

//--------------------------------------------------------
// Timer1 Overflow Interrupt Interval : 1ms       (TIMER1_CNT x DIV / CLK [s])
// Waveform Generation Mode           : Normal    (WGM1[3:0] = 0000)
// Clock Freqency (CLK)               : 16 MHz    (0.0625 us)
// Clock Select   (CLK/DIV)           : CLK / 64  (CS1[2:0] = 011)
//--------------------------------------------------------
#define TIMER1_INTERVAL_MS  1         // Timer1 Overflow Interrupt Interval [ms]

#define TIMER1_CLK_FREQ_HZ  16000000  // Clock Freqency [Hz]
#define TIMER1_CLK_SEL      8         // Clock Select (CLK/1, CLK/8, CLK/64, CLK/256, CLK/1024)


#define TIMER1_CLK_DIV_HZ   TIMER1_CLK_FREQ_HZ / TIMER1_CLK_SEL     // Timer1 Count Clock [Hz]      
#define TIMER1_CNT          TIMER1_CLK_DIV_HZ * TIMER1_INTERVAL_MS / 1000
#define TIMER1_CNT_INIT     65536 - TIMER1_CNT                      // Timer1 Count Initial Value

//--------------------------------------------------------
//  Function ProtoType
//--------------------------------------------------------
unsigned long Get_Random_Number(unsigned long min, unsigned long max, unsigned long seed);

void Get_Hit_State(unsigned char Target);
void Get_Judge_Time(unsigned char Target);

void SerialPrint_Hit_State(unsigned char Target);
void SerialPrint_Judge_Time(unsigned char Target);

void Set_Flag(unsigned char Target, unsigned char Flag1);
void Set_Mode(unsigned char Target, unsigned char Mode);
void Set_Judge_Time(unsigned char Target, unsigned long Judge1, unsigned long Judge2, unsigned long Judge3, unsigned long Judge4);
void Set_Score_(unsigned long value);

void i2c_Receive(unsigned char target);
void i2c_Transmit(unsigned char target, unsigned char data1, unsigned char data2, unsigned char data3, unsigned char data4);
void i2c_SlaveScan(void);
void DFPlayer_Init(void);
void Timer1_Init(void);

//--------------------------------------------------------
//  Global Variables
unsigned long cnt_ms;
unsigned long cnt1ms;
unsigned long cnt_s;
unsigned long cnt_s_GameTime = 0;
unsigned long rand_num;

// Push Switch
unsigned char PushSwitch;

// I2C
unsigned char i2c_SlaveAdr[UNIT] = {0x40, 0x42, 0x44, 0x46, 0x48, 0x4A};
unsigned char i2c_RxData[UNIT][4];

// Game
unsigned char mode = 0;
unsigned char Game_Flag = 0;
unsigned char Game_Active_Target = 0;
unsigned char Game_Active_Target_Old = 0;
unsigned long Game_Score = 0;
unsigned long Game_HitTime = 0;

// Target
unsigned char Hit_Flag[UNIT-1]    = {0};    // Hit Flag
unsigned char Hit_Mode[UNIT-1]    = {0};    // Hit Mode
unsigned long Hit_Time[UNIT-1]    = {0};    // Hit Time
unsigned long Judge_Time1[UNIT-1] = {0};    // Judge Time 1
unsigned long Judge_Time2[UNIT-1] = {0};    // Judge Time 2
unsigned long Judge_Time3[UNIT-1] = {0};    // Judge Time 3
unsigned long Judge_Time4[UNIT-1] = {0};    // Judge Time 4

// Demo
unsigned short Game_Score_Combo = 0;
float Combo_Multi = 1;

int Last_Mode = 1;
int Last_Time = 50;

//--------------------------------------------------------
//  Interrupt Function
//--------------------------------------------------------
ISR(TIMER1_OVF_vect)                      // Timer/Counter1 Overflow
{
  TCNT1 = TIMER1_CNT_INIT;                //  TCNT1 – Timer/Counter1
  cnt_ms++;
  cnt1ms++;
  if (cnt1ms >= 1000){
    cnt1ms = 0;
    cnt_s++;
    if (Game_Flag != 0){ cnt_s_GameTime++; }
  }
  rand_num++;
  if(rand_num > 4){
    rand_num = 0;
  }

}

//--------------------------------------------------------
//  Setup
//--------------------------------------------------------
void setup() {
  
  unsigned char t;

  // I/O
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_PSW, INPUT_PULLUP);
  pinMode(PIN_DFP_BUSY, INPUT);
   

  // I2C
  Wire.begin();
  
  // Serial Initialize
  Serial.begin(115200);
  DFPSerial.begin(9600);
  Serial.println("");

  // LED
  digitalWrite(PIN_LED, HIGH);

  // Score
  i2c_Transmit(5, 0x02, 0x05, 0x54, 0x41);    // Score : 25TA

  // Game Initailize
  Serial.println("");
  Serial.println("25TA Shooting Game Initialize...");

  // I2C Slave Scan
  i2c_SlaveScan();
  delay(3000);
  
  // Target Initailize
  for(t=0; t<=4; t++){
    Set_Mode(t, 0);                   // Mode Initaize
  }
  Serial.println("Get Judge Times");
  for(t=0; t<=4; t++){
    Get_Judge_Time(t);                // Get Judge Time
    SerialPrint_Judge_Time(t);
  }
  Serial.println("");
  Serial.println("");
  delay(1000);

  Serial.println("Set Judge Times");  
  for(t=0; t<=4; t++){
    Set_Judge_Time(t, GAME_JUDGE_TIME1, GAME_JUDGE_TIME2, GAME_JUDGE_TIME3, GAME_JUDGE_TIME4);
    Get_Judge_Time(t);
    SerialPrint_Judge_Time(t);
  }
  Serial.println("");
  Serial.println("");
  delay(1000);

  // DFPlayer Initialize
  DFPlayer_Init();
  DFPlayer.volume(30);      // Set Volume (0 to 30)
  DFPlayer.playFolder(DFP_FOLDER_01, DFP_FILE_013);
  while(digitalRead(PIN_DFP_BUSY) == 0);
  
  delay(5000);

  digitalWrite(PIN_LED, LOW);

  // Timer1 Initialize
  Timer1_Init();

}



//--------------------------------------------------------
//  Loop
//--------------------------------------------------------
void loop() {

  unsigned char t;

  digitalWrite(PIN_LED, digitalRead(PIN_DFP_BUSY));
  if (digitalRead(PIN_PSW) == 0){ PushSwitch = 0x01; } else { PushSwitch = 0x00; }


  // Game End Check
  if (Game_Flag != 0){
    if (cnt_s_GameTime >= GAME_TIME_S){
      Game_Flag = 0;
      mode = 100;
    }
    if (cnt_s_GameTime >= Last_Time && Last_Mode != 2){
      Last_Mode = 2;
      DFPlayer.playFolder(DFP_FOLDER_01, DFP_FILE_014);
    }
  }

  switch(mode){
    case 0:
      Game_Flag = 0;
      Game_Score = 0;
      cnt_s_GameTime = 0;
      if (PushSwitch != 0){
        DFPlayer.playFolder(DFP_FOLDER_01, DFP_FILE_012);
        while(digitalRead(PIN_DFP_BUSY) == 0);
        i2c_Transmit(5, 0x54, 0x45, 0x53, 0x54);    // Score : test
        for(t=0; t<=4; t++){                        // Hit Check Mode : 20
          Set_Flag(t, 20);
        }
        delay(500);
        mode = 10;
      }
      break;

    case 10:     // Gun Test
        if (PushSwitch != 0){
        i2c_Transmit(5, 0x53, 0x54, 0x42, 0x59);    // Score : Stby
        for(t=0; t<=4; t++){                        // Game Active Check : 30
          Set_Flag(t, 30);
        }
        delay(250);
        mode = 20;
      }
      if (Hit_Flag[0] != 0){ mode = 11; }
      if (Hit_Flag[1] != 0){ mode = 11; }
      if (Hit_Flag[2] != 0){ mode = 11; }
      if (Hit_Flag[3] != 0){ mode = 11; }
      if (Hit_Flag[4] != 0){ mode = 11; }
      break;
    case 11:
      DFPlayer.playFolder(DFP_FOLDER_01, DFP_FILE_004);
      if (digitalRead(PIN_DFP_BUSY) == 0){
        mode = 12;
      }
      break;
    case 12:
      if (digitalRead(PIN_DFP_BUSY) != 0){
        mode = 10;
      }
      break;

    case 20:      // Game Start Sound Play
      DFPlayer.playFolder(DFP_FOLDER_01, DFP_FILE_001);
      if (digitalRead(PIN_DFP_BUSY) == 0){
        mode = 21;
        Last_Mode = 1;
      }
      break;
    case 21:
      if (digitalRead(PIN_DFP_BUSY) != 0){
        mode = 30;
      }
      break;

    case 30:      // Game Start
      Game_Flag = 1;
      Game_Score = 0;
      Set_Score(Game_Score);
      cnt_s_GameTime = 0;
      Game_Active_Target = rand_num;
      Game_Active_Target_Old = 5;
      mode = 40;
      break;

    case 40:      // Set Active Target
      Set_Flag(Game_Active_Target, 40);    // Target Active
      Game_Active_Target_Old = Game_Active_Target;
      mode = 50;
      break;
    
    case 50:      // Hit Flag Check
      if (Hit_Flag[Game_Active_Target] != 0){
        mode = 51;
      }
      break;
    case 51:
      // DFPlayer.playFolder(DFP_FOLDER_01, DFP_FILE_003);
      // if (digitalRead(PIN_DFP_BUSY) == 0){
      mode = 52;
      // }
      break;
    case 52:
      if (digitalRead(PIN_DFP_BUSY) != 0){
        mode = 60;
      }
      break;

    case 60:      // Get Hit Time
      Game_HitTime = Hit_Time[Game_Active_Target];
      mode = 70;
      break;

    case 70:      // Calcurate Game Point
        if (Game_HitTime <= GAME_JUDGE_TIME1) {
            Combo_Add();
            Game_Score = Game_Score + GAME_POINT1 * Combo_Multi * Last_Mode;
            Combo_Sound();
        }
        else if (Game_HitTime <= GAME_JUDGE_TIME2) {
            Combo_Reset();
            Game_Score = Game_Score + GAME_POINT2 * Last_Mode;
            DFPlayer.playFolder(DFP_FOLDER_01, DFP_FILE_004);
            while(digitalRead(PIN_DFP_BUSY) == 0);
        }
        else if (Game_HitTime <= GAME_JUDGE_TIME3) {
            Combo_Reset();
            Game_Score = Game_Score + GAME_POINT3 * Last_Mode;
            DFPlayer.playFolder(DFP_FOLDER_01, DFP_FILE_005);
            while(digitalRead(PIN_DFP_BUSY) == 0);
        }
        else if (Game_HitTime <= GAME_JUDGE_TIME4) {
            Combo_Reset();
            Game_Score = Game_Score + GAME_POINT4 * Last_Mode;

            DFPlayer.playFolder(DFP_FOLDER_01, DFP_FILE_006);
            while(digitalRead(PIN_DFP_BUSY) == 0);
        }
        else {
            Combo_Reset();
            Game_Score = Game_Score + GAME_POINT4 * Last_Mode;
            DFPlayer.playFolder(DFP_FOLDER_01, DFP_FILE_007);
            while(digitalRead(PIN_DFP_BUSY) == 0);
        }
      Serial.print(Game_Score);
      Serial.println("");
      if (Game_Score > 9999){
        Game_Score = 9999;
      }
      Set_Score(Game_Score);
      delay(300);
      mode = 80;
      break;

    case 80:
      Set_Flag(Game_Active_Target, 30);    // Target Active Check (unActive)
      mode = 90;
      break;

    case 90:      // Select Taeget
      Game_Active_Target = rand_num;
      mode = 91;
      break;
    case 91:
      Game_Active_Target = rand_num;
      if(Game_Active_Target != Game_Active_Target_Old){
        mode = 92;
      }
      break;
    case 92:
      mode = 40;
      break;

    case 100:     // Game End
      DFPlayer.playFolder(DFP_FOLDER_01, DFP_FILE_002);
      if (digitalRead(PIN_DFP_BUSY) == 0){
        mode = 101;
      }
      break;
    case 101:
      if (digitalRead(PIN_DFP_BUSY) != 0){
        mode = 110;
      }
      break;

    case 110:       // Game Score
      if (cnt_ms <= 500){
        Set_Score(Game_Score);
      }
      else{
        i2c_Transmit(5, 0x20, 0x20, 0x20, 0x20);    // Score : Stby
      }
      if (cnt_ms >= 1000){
        cnt_ms = 0;
      }
      if (PushSwitch != 0){
        i2c_Transmit(5, 0x02, 0x05, 0x54, 0x41);    // Score : 25TA
        for(t=0; t<=4; t++){                        // Standby : 10
          Set_Flag(t, 10);
        }
        delay(250);
        mode = 0;
      }
      break;



  }
  
  
  Get_Hit_State(0);
  Get_Hit_State(1);
  Get_Hit_State(2);
  Get_Hit_State(3);
  Get_Hit_State(4);

  SerialPrint_Hit_State(0);
  SerialPrint_Hit_State(1);
  SerialPrint_Hit_State(2);
  SerialPrint_Hit_State(3);
  SerialPrint_Hit_State(4);

  // Serial.print(" Random=");
  // Serial.print(rand_num);

  // Serial.print(" mode=");
  // Serial.print(mode);  
  
  // Serial.println("");


//  Set_Judge_Time(0, 123, 456, 789, 1011);
/*
  Get_Judge_Time(0);
  Get_Judge_Time(1);
  SerialPrint_Judge_Time(0);
  SerialPrint_Judge_Time(1);
  SerialPrint_Judge_Time(2);
  SerialPrint_Judge_Time(3);
  SerialPrint_Judge_Time(4);
  Serial.println("");
*/

  //Set_Score(Hit_Time[0]);
  //Set_Score(cnt_s_GameTime);

}

//--------------------------------------------------------
//  Combo System
//--------------------------------------------------------
void Combo_Add(){
  Game_Score_Combo ++;
  if (Game_Score_Combo == 1 ||Game_Score_Combo == 0){
    Combo_Multi = 1;
  }
  else if (Game_Score_Combo == 2){
    Combo_Multi = 1.2;
  }
  else if (Game_Score_Combo == 3){
    Combo_Multi = 1.5;
  }
  else if (Game_Score_Combo == 4){
    Combo_Multi = 1.8;
  }
  else{
    Combo_Multi = 2;
  }
}

void Combo_Reset(){
  Game_Score_Combo = 0;
  Combo_Multi = 1;
}

void Combo_Sound(){
  if (Game_Score_Combo == 1 ||Game_Score_Combo == 0){
    DFPlayer.playFolder(DFP_FOLDER_01, DFP_FILE_003);
    while(digitalRead(PIN_DFP_BUSY) == 0);
  }
  else if (Game_Score_Combo == 2){
    DFPlayer.playFolder(DFP_FOLDER_01, DFP_FILE_011);
    while(digitalRead(PIN_DFP_BUSY) == 0);
  }
  else if (Game_Score_Combo == 3){
    DFPlayer.playFolder(DFP_FOLDER_01, DFP_FILE_010);
    while(digitalRead(PIN_DFP_BUSY) == 0);
  }
  else if (Game_Score_Combo == 4){
    DFPlayer.playFolder(DFP_FOLDER_01, DFP_FILE_008);
    while(digitalRead(PIN_DFP_BUSY) == 0);
  }
  else{
    DFPlayer.playFolder(DFP_FOLDER_01, DFP_FILE_009);
    while(digitalRead(PIN_DFP_BUSY) == 0);
  }
}

//--------------------------------------------------------
//  Get Hit State
//--------------------------------------------------------
//  Target : 0 ～ 4
//--------------------------------------------------------
void Get_Hit_State(unsigned char Target)
{
  unsigned long cnt;

  i2c_Transmit(Target, 0x10, 0x00 , 0x00, 0x00);
  i2c_Receive(Target);
  Hit_Flag[Target] = i2c_RxData[Target][0];
  Hit_Mode[Target] = i2c_RxData[Target][1];
  cnt = i2c_RxData[Target][2] << 8;
  cnt = cnt | i2c_RxData[Target][3];  
  Hit_Time[Target] = cnt;
}

void SerialPrint_Hit_State(unsigned char Target)
{
  // Serial.print(Target);
  // Serial.print(": ");
  // Serial.print(Hit_Flag[Target]);
  // Serial.print(" ");
  // Serial.print(Hit_Mode[Target]);
  // Serial.print(" ");
  // Serial.print(Hit_Time[Target]);
  // Serial.print("  ");
  ;
}

//--------------------------------------------------------
//  Get Judge Time
//--------------------------------------------------------
void Get_Judge_Time(unsigned char Target)
{
  unsigned long cnt;

  i2c_Transmit(Target, 0x12, 0x00, 0x00, 0x00);
  i2c_Receive(Target);
  cnt = i2c_RxData[Target][0] << 8;
  cnt = cnt | i2c_RxData[Target][1];  
  Judge_Time1[Target] = cnt;

  cnt = i2c_RxData[Target][2] << 8;
  cnt = cnt | i2c_RxData[Target][3];
  Judge_Time2[Target] = cnt;

  i2c_Transmit(Target, 0x13, 0x00, 0x00, 0x00);
  i2c_Receive(Target);
  cnt = i2c_RxData[Target][0] << 8;
  cnt = cnt | i2c_RxData[Target][1];  
  Judge_Time3[Target] = cnt;

  cnt = i2c_RxData[Target][2] << 8;
  cnt = cnt | i2c_RxData[Target][3];
  Judge_Time4[Target] = cnt;

}

void SerialPrint_Judge_Time(unsigned char Target)
{
  Serial.print(Target);
  Serial.print(": ");
  Serial.print(Judge_Time1[Target]);
  Serial.print(" ");
  Serial.print(Judge_Time2[Target]);
  Serial.print(" ");
  Serial.print(Judge_Time3[Target]);
  Serial.print(" ");
  Serial.print(Judge_Time4[Target]);
  Serial.print("  "); 
}

//--------------------------------------------------------
//  Set Flag
//--------------------------------------------------------
void Set_Flag(unsigned char Target, unsigned char Flag1)
{
  unsigned char D1 = 0x20;
  unsigned char D2 = Flag1;
  unsigned char D3 = 0;       // Flag2
  unsigned char D4 = 0;       // Flag3
  i2c_Transmit(Target, D1, D2, D3, D4);
}

//--------------------------------------------------------
//  Set Mode
//--------------------------------------------------------
void Set_Mode(unsigned char Target, unsigned char Mode)
{
  unsigned char D1 = 0x30;
  unsigned char D2 = Mode;
  unsigned char D3 = 0x00;
  unsigned char D4 = 0x00;
  i2c_Transmit(Target, D1, D2, D3, D4);
}

//--------------------------------------------------------
//  Set Judge Time
//--------------------------------------------------------
void Set_Judge_Time(unsigned char Target, unsigned long Judge1, unsigned long Judge2, unsigned long Judge3, unsigned long Judge4)
{
  unsigned long t;
  unsigned char D1;
  unsigned char D2;
  unsigned char D3;
  unsigned char D4;

  t = Judge1;
  D1 = 0x21;
  D2 = (unsigned char)((t >> 8) & 0x00FF);
  D3 = (unsigned char)(t & 0x00FF);
  D4 = 0;
  i2c_Transmit(Target, D1, D2, D3, D4);

  t = Judge2;
  D1 = 0x22;
  D2 = (unsigned char)((t >> 8) & 0x00FF);
  D3 = (unsigned char)(t & 0x00FF);
  D4 = 0;
  i2c_Transmit(Target, D1, D2, D3, D4);

  t = Judge3;
  D1 = 0x23;
  D2 = (unsigned char)((t >> 8) & 0x00FF);
  D3 = (unsigned char)(t & 0x00FF);
  D4 = 0;
  i2c_Transmit(Target, D1, D2, D3, D4);

  t = Judge4;
  D1 = 0x24;
  D2 = (unsigned char)((t >> 8) & 0x00FF);
  D3 = (unsigned char)(t & 0x00FF);
  D4 = 0;
  i2c_Transmit(Target, D1, D2, D3, D4);
}

//--------------------------------------------------------
//  Set Score
//--------------------------------------------------------
void Set_Score(unsigned long value)
{
  unsigned long v;
  unsigned char D1;
  unsigned char D2;
  unsigned char D3;
  unsigned char D4;

  if (value <= 9999){
    v = value;
    D4 = (unsigned char)(v%10);
    D3 = (unsigned char)((v%100)/10);
    D2 = (unsigned char)((v%1000)/100);
    D1 = (unsigned char)(v/1000);
    i2c_Transmit(5, D1, D2, D3, D4);
  }
  else{
    i2c_Transmit(5, 0xFF, 0xFF, 0xFF, 0xFF);
  }
}

//--------------------------------------------------------
//  I2C Receive
//--------------------------------------------------------
void i2c_Receive(unsigned char target)
{
  Wire.requestFrom(i2c_SlaveAdr[target], 4);
  i2c_RxData[target][0] = Wire.read();
  i2c_RxData[target][1] = Wire.read();
  i2c_RxData[target][2] = Wire.read();
  i2c_RxData[target][3] = Wire.read();
  Wire.endTransmission( );
}

//--------------------------------------------------------
//  I2C Transmit
//--------------------------------------------------------
void i2c_Transmit(unsigned char target, unsigned char data1, unsigned char data2, unsigned char data3, unsigned char data4)
{
  Wire.beginTransmission(i2c_SlaveAdr[target]);
  Wire.write(data1);
  Wire.write(data2);
  Wire.write(data3);
  Wire.write(data4);
  Wire.endTransmission( ); 
}

//--------------------------------------------------------
//  I2C Scan
//--------------------------------------------------------
void i2c_SlaveScan(void)
{
  byte  error;
  byte  address;
  int   nDevices;
  int   m;

  Serial.println("");                                // ヘッダー表示
  Serial.println("I2C Slave Address Scan...");
  Serial.println("");
  Serial.print("    ");
  for (m = 0; m < 16; m++) {
    Serial.print(" ");
    Serial.print(m, HEX);
    Serial.print(" ");
  }
  Serial.println("");

  nDevices = 0;                                       // 検出デバイス数カウンタクリア

  // アドレス スキャン
  for (address = 0; address <= 127; address++) {      //  アドレス範囲：0 ～ 127
    Wire.beginTransmission(address);                  //  I2C 送信(アドレス）
    error = Wire.endTransmission();                   //  I2C 送信終了

    if ( (address % 16) == 0) {                       // アドレス先頭値表示
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);
      Serial.print(":");
    }

    switch (address) {
      case 0x00 ... 0x03:                             // 予約アドレス
      case 0x78 ... 0x7F:
        Serial.print("   ");
        break;
      default:                                        // 使用可能アドレス範囲
        switch (error) {
          case 0:                                     // 　通信成功検出、検出アドレスを表示
            Serial.print(" ");
            if (address < 16) {
              Serial.print("0");
            }
            Serial.print(address, HEX);
            nDevices++;
            break;
          case 4:                                     // 　その他のエラー検出時の表示
            Serial.print(" **");
            break;
          default:                                    // 　上記以外の表示
            Serial.print(" --");
        }
    }

    if ((address > 0) && ((address + 1) % 16 == 0)) { // 改行条件
      Serial.println("");
    }
  }

  Serial.println("");                                 // I2Cスレーブデバイス数表示
  Serial.print("I2C Slave Device = ");
  Serial.print(nDevices);
  Serial.println("");
  Serial.println("");  
}

//--------------------------------------------------------
//  DFPlayer Initialize
//--------------------------------------------------------
void DFPlayer_Init(void)
{
  if (!DFPlayer.begin(DFPSerial, true, true)){
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    // while(true);   
  }
  Serial.println(F("DFPlayer Mini Active."));
}

//--------------------------------------------------------
//  Timer 1 Initialize
//--------------------------------------------------------
void Timer1_Init(void)
{
  TCCR1A = 0x00;                          // TCCR1A – Timer/Counter1 Control Register A
 
  TCCR1B = 0x00;                          // TCCR1B – Timer/Counter1 Control Register B
  #if   (TIMER1_CLK_SEL == 1)             //   CS1[2:0] = 001 : clkI/O/1 (no prescaling)        16MHz     (0.0625 us)   16000
    TCCR1B |= (1 << CS10);
  #elif (TIMER1_CLK_SEL == 8)             //   CS1[2:0] = 010 : clkI/O/8 (from prescaler)       2MHz      (0.5 us)      2000
    TCCR1B |= (1 << CS11);
  #elif (TIMER1_CLK_SEL == 64)            //   CS1[2:0] = 011 : clkI/O/64 (from prescaler)      250kHz    (4 us)        250
    TCCR1B |= (1 << CS11) | (1 << CS10);
  #elif (TIMER1_CLK_SEL == 256)           //   CS1[2:0] = 100 : clkI/O/256 (from prescaler)     62.5kHz   (16 us)       62.5
    TCCR1B |= (1 << CS12);
  #elif (TIMER1_CLK_SEL == 1024)          //   CS1[2:0] = 101 : clkI/O/1024 (from prescaler)    15.625kHz (64 us)       15.625
    TCCR1B |= (1 << CS12) | (1 << CS10);
  #endif


  TCNT1 = TIMER1_CNT_INIT;                // TCNT1 – Timer/Counter1
 
  TIMSK1 = 0x00;                          // TIMSK1 – Timer/Counter1 Interrupt Mask Register
  TIMSK1 |= (1 << TOIE1);                 //  TOIE1: Timer/Counter1, Overflow Interrupt Enable
}
