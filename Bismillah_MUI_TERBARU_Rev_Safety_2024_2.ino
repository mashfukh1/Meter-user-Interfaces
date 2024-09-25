#include <Arduino.h>
#include <U8g2lib.h>   // libray LCD


#define MAX_BITS 100                 // max number of bits
#define WEIGAND_WAIT_TIME  3000      // time to wait for another weigand pulse. 

unsigned char databits[MAX_BITS];    // stores all of the data bits
unsigned char bitCount;              // number of bits currently captured
unsigned char flagDone;              // goes low when data is currently being captured
unsigned int weigand_counter;        // countdown until we assume there are no more bits

unsigned long facilityCode=0;        // decoded facility code
unsigned long cardCode=0; 
String cardCodes; // decoded card code

// interrupt that happens when INTO goes low (0 bit)

void ISR_INT0()
{
  //Serial.print("0");   // uncomment this line to display raw binary
  bitCount++;
  flagDone = 0;
  weigand_counter = WEIGAND_WAIT_TIME; 
 
}

// interrupt that happens when INT1 goes low (1 bit)
void ISR_INT1()
{
  //Serial.print("1");   // uncomment this line to display raw binary
  databits[bitCount] = 1;
  bitCount++;
  flagDone = 0;
  weigand_counter = WEIGAND_WAIT_TIME; 
}

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>  // SPI protocol
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>  // 12C protrocol
#endif

const unsigned long eventInterval = 120000;
unsigned long previousTime = 0;

// process status
String waitingStatus = "0";
String processStatus = "1";
String witingToStart = "2";
String successStatus = "3";
String failedStatus  = "4";
String timeoutStatus = "5";
String successStatusManual = "6";
String muiStatus = "10";



// batch status 
String noBatch       = "0";
String batchProgress = "1";
String batchStopped  = "2";
String dataAvailable = "3";
String terminate     = "4";
String disconnect    = "5";
String connected     = "6";
String loading       = "7";
String selesai       = "8";
String beforeFinish  = "9";
String bottomBreak   = "20";
String bottomPlug    = "30";

// 
String percent; // menampung dan mengkonversi data interger ke string untuk percent 0% - 100% (0L - 8000L)
String volume; // menampung dan mengkonversi data interger ke string untuk percent 0L - 8000L
String preset; // satuan yang akan di isi. (8000, 10000, etc)
String THeader = "*";
String nopol;
String lo;

/////////// tidak dipakai //////////// 
String dataResult; 
int presetInt = 0;

int ID = 1;
int data = 0;
int convStatus;
//////////// tidak dipakai //////////////

//"R" = belok kanan   (HP > MIKRO) 
//"L" = belok kiri 

//@,3,8000,3456,1,@//   data yang deterima  server > MUI

/*
@ =  */

String header = "@";   
String separator = ",";
bool cardState = false;

/////////////////////////////////////////
char controlFlow = 'A';
char controlCard = 'A';

String codeNewTrans = "100";
String codeExTrans  = "300";
String codeApprove  = "200";
String codeCancel   = "400";
String codeFinish   = "500";
String codeFirstReq = "600";
String closeString;
//////////////////////////////////////////


String inputStringState = ""; // buffer untuk menampung data  dari server sebelum proses splitting 
String inputString1 = ""; // buffer untuk menampung data  dari server sebelum proses splitting 
String message = ""; // buffer untuk menampung data  dari server sebelum proses splitting 


//////////////////////////////////
bool cardFailBack;
bool safetyEvent;

U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0,13,11,10,16); // untuk konfigurasi pin arduino ke LCD (referensi schematic eagle)

/////////////////RFID/////////////////////////////////////////////////  
// merah   = VCC +12V
// hitam   = GND
// kuning  = BEEP
// putih   = D1/RX/4R+
// hijau   = D0/TX/4R-
// abu abu = WG26/34
// biru    = LED


void setup(void) {

  Serial.begin(9600); // konfigurasi baudrate serial 0 (serial monitor / debugging)
  Serial3.begin(9600); // konfigurasi baudrate serial 3 (komunikasi ke server)
  
  inputString1.reserve(200);  // kapasitas memory internal 200 B
  inputStringState.reserve(20); //20B

  pinMode(19, INPUT);     // DATA0 (INT0)
  pinMode(18, INPUT);     // DATA1 (INT1)
  
  attachInterrupt(2, ISR_INT0, FALLING); 
  attachInterrupt(3, ISR_INT1, FALLING);
  
  weigand_counter = WEIGAND_WAIT_TIME;
  
  u8g2.begin(); // memulai komunikasi LCD ke MIkro

  u8g2.firstPage();
    do{
      starting(); // untuk menampilkan halaman awal, 
      }
      while(u8g2.nextPage());

   delay(2000);
   mainFrame();
   cardFailBack = false; 
   safetyEvent = false;
}


void loop() 
{
 mainFrame();
 readCard();
}

void mainFrame(){ 
while(bitCount < 1)
{
unsigned long currentTime = millis();
String header,val,fix,pin,datas,bar,cardStatus,batchState;
int valPercent,valBar;
int valIn = 0;

if(Serial3.available()>0){  
     message =  Serial3.readStringUntil('\n'); // penampungan data serial3
     inputString1 =  message; // penampungan data dari variabe message ke inpurstring
    // previousTime = currentTime;
     Serial.println(message);
  }

//else{
//   if (currentTime - previousTime >= eventInterval) {    
//       inputString1 = "@,5,8000,9811237923,1,BH3456SFU,@";
//   }
// inputString1 = "@,1,8000,1000,7,576256527,BH3456SFU,@";

//}
 
 u8g2.firstPage();
  do{  
      //@,3,8000,3456,1,@     
      header        = getValue(inputString1,',',0);
      cardStatus    = getValue(inputString1,',',1);
      preset        = getValue(inputString1,',',2);
      val           = getValue(inputString1,',',3);
      batchState    = getValue(inputString1,',',4);
      lo            = getValue(inputString1,',',5);
      nopol         = getValue(inputString1,',',6);
      closeString   = getValue(inputString1,',',7);

      if(header == "@" && cardStatus == successStatus){ // jika process status (success)
          valIn += val.toInt();
          presetInt = preset.toInt();
          valPercent = map(valIn, 0,presetInt, 0, 100);
          valBar = map(valIn,0,presetInt, 64, 14);
          percent = String(valPercent);
      
          u8g2.clearBuffer(); 
          u8g2.setFont(u8g2_font_tinytim_tf);
          u8g2.drawStr(2,10,"Status :");
          u8g2.setFont(u8g2_font_tinytim_tf);
          u8g2.setCursor(42,10);
          u8g2.print(batchStatus(batchState));
          u8g2.setFont(u8g2_font_tinytim_tf);
          u8g2.drawStr(3,20,"Preset:");
          u8g2.setFont(u8g2_font_tinytim_tf);
          u8g2.setCursor(42,20);
          u8g2.print(preset);
          u8g2.setFont(u8g2_font_tenthinguys_tf);
          u8g2.drawStr(3,40,"Loaded");
          u8g2.setFont(u8g2_font_luBS18_tn);
          u8g2.setCursor(0,64);
          u8g2.print(val);
          u8g2.setFont(u8g2_font_tinytim_tf);
          u8g2.setCursor(95,10);
          u8g2.print(percent);
          u8g2.setFont(u8g2_font_tinytim_tf);
          u8g2.setCursor(109,10);
          u8g2.print("%");
          u8g2.drawFrame(80,14,45,50); 
          u8g2.drawBox(80,valBar,45,50); // full 14 low 64

      }
      else if(header == "@" && cardStatus == successStatusManual){ // jika process status (success)

          valIn += val.toInt();
          presetInt = preset.toInt();
          valPercent = map(valIn, 0,presetInt, 0, 100);
          valBar = map(valIn,0,presetInt, 64, 14);
          percent = String(valPercent);
      
          u8g2.clearBuffer(); 
          u8g2.setDrawColor(1);
          u8g2.setFont(u8g2_font_tinytim_tf);
          u8g2.drawStr(3,10,"NOPOL  :");
          u8g2.setFont(u8g2_font_tinytim_tf);
          u8g2.setCursor(48,10);
          u8g2.print(nopol);
          u8g2.setFont(u8g2_font_tinytim_tf);
          u8g2.drawStr(3,20,"LO     :");
          u8g2.setFont(u8g2_font_tinytim_tf);
          u8g2.setCursor(48,20);
          u8g2.print(lo);
          u8g2.setFont(u8g2_font_tinytim_tf);
          u8g2.drawStr(3,30,"PRESET :");
          u8g2.setFont(u8g2_font_tinytim_tf);
          u8g2.setCursor(48,30);
          u8g2.print(preset);
          u8g2.drawFrame(0,0,128,64);

          if(batchState == loading)
           {
             showNotiv(true);
           }
           else if (batchState == beforeFinish){
             beforeFinishing();
           }
           else if(batchState == bottomBreak){
             starting3();
           }
           else if(batchState == bottomPlug){
             starting2();
           }
           else{
             showNotiv(false);
             //Serial.println("SELESAI");
           }
      }

      else if(header == "@" && cardStatus == failedStatus && cardFailBack == false){
          failCardFalse();
      }
     
      else if(header == "@" && cardStatus == failedStatus && cardFailBack == true){
          failCardTrue();
      }
      else if(header == "@" && cardStatus == processStatus){
          processCard();
      }
      
      else if(header == "@" && cardStatus == muiStatus){
          Serial3.print("I AM READY ");
          delay(500);
          inputString1 = "$$";
      }
      else if(header == "@" && cardStatus == timeoutStatus){
          timeOut();
      }
      
      else {
       u8g2.setDrawColor(1); 
       u8g2.setFont(u8g2_font_tenthinguys_tf);
       u8g2.setCursor(29,29);
       u8g2.print("SILAHKAN");
       u8g2.setCursor(25,47);
       u8g2.print("TAP KARTU");
       //Serial.println("SILAHKAN TAP KARTU");
       u8g2.drawFrame(0,0,128,64);  
     }    
    }
  while(u8g2.nextPage()); 
  
}
}

void showNotiv(bool state){
  if (state == true){
    static bool blinkState = false; // Track the blink state
      if (blinkState) {
        u8g2.setDrawColor(1); 
        u8g2.drawBox(0,34,128,34);
        u8g2.setFont(u8g2_font_ncenB14_tf);  // Select a bold font
        u8g2.setDrawColor(0); // Set text color to black
        u8g2.drawStr(8, 57,"PENGISIAN"); // Draw string (x, y, text)
      }
  blinkState = !blinkState;
  u8g2.sendBuffer();
  delay(500);
  }
  else{
        u8g2.drawBox(0,34,128,34);
        u8g2.setFont(u8g2_font_ncenB14_tf);  // Select a bold font
        u8g2.setDrawColor(0); // Set text color to black
        u8g2.drawStr(24, 57,"SELESAI"); // Draw string (x, y, text)
        u8g2.sendBuffer();
  }
}
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;
 
  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  } 
 
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void starting(void) {
  u8g2.clearBuffer(); 
  u8g2.setFont(u8g2_font_helvB24_tf);
  u8g2.drawStr(33,30,"MUI");
  u8g2.setFont(u8g2_font_mozart_nbp_tf);
  u8g2.drawStr(2,50,"Meter User Interfaces");
  u8g2.setFont(u8g2_font_u8glib_4_hr);
  u8g2.drawStr(8,63,"CITIA ENGINEERING INDONESIA");
  u8g2.sendBuffer();
  delay(1000);  
}

void starting2(){
  u8g2.firstPage();
    do{
  u8g2.setDrawColor(1); 
  u8g2.setFont(u8g2_font_tenthinguys_tf);
  u8g2.setCursor(29,20);
  u8g2.print("PASTIKAN");
  u8g2.setCursor(9,38);
  u8g2.print("BOTTOM LOADER");
  u8g2.setCursor(24,56);
  u8g2.print("TERPASANG");
  u8g2.drawFrame(1,1,127,63);
  //Serial.println("PASTIKAN BOTTOM LOADER TERPASANG");
      }
     while(u8g2.nextPage());  
}
void starting3(){
  u8g2.firstPage();
    do{
  u8g2.setDrawColor(1); 
  u8g2.setFont(u8g2_font_tenthinguys_tf);
  u8g2.setCursor(29,20);
  u8g2.print("PASTIKAN");
  u8g2.setCursor(9,38);
  u8g2.print("BOTTOM LOADER");
  u8g2.setCursor(26,56);
  u8g2.print("TERLEPAS");
  u8g2.drawFrame(1,1,127,63);
  //Serial.println("PASTIKAN BOTTOM LOADER TERPASANG");
      }
     while(u8g2.nextPage());  
}

void beforeFinishing(){
  u8g2.firstPage();
    do{
  u8g2.setDrawColor(1); 
  u8g2.setFont(u8g2_font_tenthinguys_tf);
  u8g2.setCursor(24,20);
  u8g2.print("SELESAIKAN");
  u8g2.setCursor(27,38);
  u8g2.print("TRANSAKSI");
  u8g2.setCursor(21,56);
  u8g2.print("SEBELUMNYA");
  u8g2.drawFrame(1,1,127,63);
  //Serial.println("PASTIKAN BOTTOM LOADER TERPASANG");
      }
     while(u8g2.nextPage());  
}
 
 void failCardTrue(){
   u8g2.firstPage();
    do{
       u8g2.setDrawColor(1); 
       u8g2.setFont(u8g2_font_tenthinguys_tf);
       u8g2.setCursor(29,29);
       u8g2.print("SILAHKAN");
       u8g2.setCursor(9,47);
       u8g2.print("TAP KARTU LAGI");
       //Serial.println("SILAHKAN TAP KARTU LAGI");
       u8g2.drawFrame(0,0,128,64);
      }
     while(u8g2.nextPage());     
  }

 void timeOut(){
   u8g2.firstPage();
    do{
       u8g2.setFont(u8g2_font_tenthinguys_tf);
       u8g2.setCursor(32,29);
       u8g2.print("KONEKSI");
       u8g2.setCursor(28,47);
       u8g2.print("TERPUTUS");
       //Serial.println("KONEKSI TERPUTUS");
       u8g2.drawFrame(0,0,128,64);
      }
     while(u8g2.nextPage());     
  }

void readCard()
 {
  if (!flagDone) {
    if (--weigand_counter == 0)
      flagDone = 1;
  }
  // if we have bits and we the weigand counter went out
  if (bitCount > 0 && flagDone) {
    unsigned char i;
    
  if (bitCount == 26)
    {
      // standard 26 bit format
      // facility code = bits 2 to 9
      for (i=1; i<9; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
     
      // card code = bits 10 to 23
      for (i=9; i<25; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }
      printCode();
    }
     bitCount = 0;
     facilityCode = 0;
     cardCode = 0;
     for (i=0; i<MAX_BITS; i++)
     {
       databits[i] = 0;
     }  
  }  
  }
int printCode(){ 
 if (cardCode > 0){ 
  
  dataResult = THeader+","+
                 String(cardCode)+","+
                 "0"+","+
                 "0"+","+
                 THeader;
  
  u8g2.firstPage();
  
    do{
      u8g2.setDrawColor(1); 
      u8g2.setFont(u8g2_font_tenthinguys_tf);
      u8g2.setCursor(29,29);
      u8g2.print("PIN MASUK");
      u8g2.setFont(u8g2_font_tenthinguys_tf);
      u8g2.setCursor(32,47);
      u8g2.print("ID: "+String(cardCode));
      u8g2.drawFrame(0,0,128,64);
      Serial3.println(dataResult);
      }
     while(u8g2.nextPage()); 
     delay(3000);
     cardState == true;
   }
}
void failCardFalse(){
   u8g2.firstPage();
    do{
      u8g2.setDrawColor(1); 
      u8g2.setFont(u8g2_font_tenthinguys_tf);
      u8g2.setCursor(50,29);
      u8g2.print("PIN");
      u8g2.setFont(u8g2_font_tenthinguys_tf);
      u8g2.setCursor(10,47);
      u8g2.print("TIDAK DIKENALI");
      //Serial.println("PIN TIDAK DIKENALI");
      u8g2.drawFrame(0,0,128,64); 
      }
     while(u8g2.nextPage());  
    delay(5000);    
    cardFailBack = true;
  }
  
 void processCard(){
        drawerAnimation();
  }

 String batchStatus(String code){
 
  String result;
  
  if (code == noBatch){
    
    result = "START";
    }
  else if (code == batchProgress){
    result = "PROSES";
    }
  else if (code == batchStopped){
    result = "SELESAI";
  }
  else if (code == dataAvailable){
    result = "TERSEDIA";
  }
  else if (code == terminate){
    result = "TERHENTI";
  }
  else if (code == connected){
    result = "TERHUBUNG";
  }
  else if (code == disconnect){
    result = "MENGHUBUNGKAN";
  }
  else{
    result = "GAGAL";
    }
   return result;
 }
 void drawerAnimation() {
  for (int xPos = 0; xPos <= 25; xPos += 2) { // Move arrow to the right
    u8g2.firstPage();
    do {
      drawAnimation(xPos);
    } while (u8g2.nextPage());
    delay(40); // Adjust to control animation speed
  }
}

void drawAnimation(int xPos) {
  u8g2.setDrawColor(1); 
  u8g2.setFont(u8g2_font_tenthinguys_tf);
  u8g2.setCursor(13,29);
  u8g2.print("HARAP TUNGGU");
  u8g2.drawFrame(0,0,128,64);
  // Draw the arrow
  u8g2.drawLine(40 + xPos, 47, 36 + xPos, 43); // Upper arrowhead
  u8g2.drawLine(40 + xPos, 47, 36 + xPos, 51); // Lower arrowhead
  u8g2.drawLine(40 + xPos-1, 47, 36 + xPos-1, 43); // Upper arrowhead
  u8g2.drawLine(40 + xPos-1, 47, 36 + xPos-1, 51); // Lower arrowhead
  u8g2.drawLine(40 + xPos-2, 47, 36 + xPos-2, 43); // Upper arrowhead
  u8g2.drawLine(40 + xPos-2, 47, 36 + xPos-2, 51); // Lower arrowhead
  u8g2.drawLine(40 + xPos-3, 47, 36 + xPos-3, 43); // Upper arrowhead
  u8g2.drawLine(40 + xPos-3, 47, 36 + xPos-3, 51); // Lower arrowhead

  u8g2.drawLine(40 + xPos-5, 47, 36 + xPos-5, 43); // Upper arrowhead
  u8g2.drawLine(40 + xPos-5, 47, 36 + xPos-5, 51); // Lower arrowhead
  u8g2.drawLine(40 + xPos-6, 47, 36 + xPos-6, 43); // Upper arrowhead
  u8g2.drawLine(40 + xPos-6, 47, 36 + xPos-6, 51); // Lower arrowhead
  u8g2.drawLine(40 + xPos-7, 47, 36 + xPos-7, 43); // Upper arrowhead
  u8g2.drawLine(40 + xPos-7, 47, 36 + xPos-7, 51); // Lower arrowhead
  u8g2.drawLine(40 + xPos-8, 47, 36 + xPos-8, 43); // Upper arrowhead
  u8g2.drawLine(40 + xPos-8, 47, 36 + xPos-8, 51); // Lower arrowhead
  
  // Draw the computer (rectangle)

  u8g2.drawBox(68, 37, 15, 13); // Stand part
  u8g2.drawBox(65, 53, 21, 5); // Stand part
}
