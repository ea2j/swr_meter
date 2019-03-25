/*
 * Sketch para el monitor de potencia y ROE publicado en los números se febrero, marzo y abril de la
 * revista de la URE. Incluye la fecha y la hora generados en el RTC DS3231.
 * v 1.0, marzo de 2019, por Arsenio "Enio" Gutiérrez, EA2HW. Software libre,
 * puede ser estudiado, modificado y utilizado libremente con cualquier fin y redistribuido sin o
 * con cambios o/y mejoras. El autor no se hace responsable de cualquier daño o perjuicio causado 
 * por el uso de este software
 */

//Las librería Wire.h está incluida en la biblioteca del IDE. Las librerías del puerto I2C para el display 2004 y
//la del módulo RTC DS3231 se deben descargar e incluir en la biblioteca
#include <RTClib.h>             //https://github.com/adafruit/RTClib
#include <LiquidCrystal_I2C.h>  //https://github.com/fdebrabander?tab=repositories
#include <Wire.h>

#define pinDIR 0
#define pinREF 1
#define screenTX 0
#define screenRX 1

//Variables globales
int dayInitial;             //Día de inicio de la sesión para el cambio de fecha en pantalla
int screenType = screenRX;  //Mantiene la pantalla antual (filas 2 y 3, información RF)

//Se definen todas las constantes que se van a utilizar
const int rSAMPLES = 1111;        //Numero de muestras que toma en un ciclo de lectura
const float rONE = 4740.0;        //Divisor de tensión R1
const float rTWO = 10720.0;       //Divisor de tensión R2
const float trsFACTOR = 25.0;     //Factor de relación de espiras en los transformadores T1 y T2
const float rmsFACTOR = sqrt(2);  //Factor de conversión de la tensión de pico a RMS
const float maxVOLTS = 5.0;       //Maxima tensión admisible en los pines de ARduino 
const float rawSTEPS = 1023.0;    //Número total de saltos digitales del ADC de 10bit de las entyradas analógicas
const float dropDIODE = 0.3;      //Perdida de tensión en el diodo schotty rectificador 1N5711
const float zANTENNA = 50.0;      //Impedancoa de la antena

//Definición de los caracteres espediales para la formación de la barra de ROE
byte b_one[8] = {B10000, B10000, B10000, B10000, B10000, B10000, B10000};   //Una barra
byte b_two[8] = {B11000, B11000, B11000, B11000, B11000, B11000, B11000};   //Dos barras... etc.
byte b_three[8] = {B11100, B11100, B11100, B11100, B11100, B11100, B11100};
byte b_four[8] = {B11110, B11110, B11110, B11110, B11110, B11110, B11110};
byte b_five[8] = {B11111, B11111, B11111, B11111, B11111, B11111, B11111};

//La clase Chronometer controla lor tiempos y pausas del flujo
class Chronometer{
        public:
        Chronometer();
        void tick();
        void reset();
        boolean havePassed(const int seconds);
        private:
        unsigned long _currentTick;
        unsigned long _lastTick;
};

Chronometer::Chronometer() {  //Inicia el cronómetro
    _lastTick = millis();
    _currentTick = millis();
};

void Chronometer::tick() {   //Captura los milisegundos iniciales
    _currentTick = millis();
};

void Chronometer::reset() {  //Iguala los milisegundos iniciales y actuales
    _lastTick = _currentTick;
};

boolean Chronometer::havePassed(const int seconds) {     //Devuelve si o no han pasado los segundos indicados
    return _currentTick >= _lastTick + (seconds * 1000);
};

LiquidCrystal_I2C lcd(

0x3f, 20, 4);
RTC_DS3231 rtc;
Chronometer chrono;

/**************************************/
/**********    SUBRUTINAS    **********/
/**************************************/

//Toma (rawLecturas) muestras raw de los pines análogicos de lectura de tensión directa y reflejada y devuelve la mas alta
int ReadSamples(int pin) {  //Toma rawLecturas muestras en A0 (pinDIR) = Directa y A1 (pinREF) = Reflejada y devuelve ña lectura más alta
    int rawMax = 0;

    for (int i = 0; i < rSAMPLES; i++) {
        int raw = analogRead(pin);

        if (raw > rawMax) {
            rawMax = raw;
        }
    }
    return rawMax;
}

//Calcula los Voltios rms de la RF directa y reflejada de la linea de transmision
float CalcVolts(int raw) {
    float voltsDivider = rTWO / (rONE + rTWO);                                      //Divisor resistivo
    float voltsPeak = (((raw * maxVOLTS) / rawSTEPS) / voltsDivider) + dropDIODE;   //Voltios pico

    return (voltsPeak * trsFACTOR) / rmsFACTOR;                                   //Voltios RMS en la línea
}

//Calcula la ROE = (voltios directa + voltios reflejada) / (voltios directa - voltios reflejada)
float CalcSwr(float fwd, float rev) {
    return (fwd + rev) / (fwd - rev);
}

//Calcula la potencia = Cuadrado de la tensión rms partido por la impedancia de la antena
float CalcPower(float fwd, float rev) {                    //Potencia = cuadrado de la tensión partido pr la impedancia de la antena

    return (pow(fwd, 2) / zANTENNA) + (pow(rev, 2) / zANTENNA); //Suma de la potencia directa y reflejada
}

//Ajusta el tiempo del RTC con los datos de las variables si la función está desmarcada
//Ajustar el tiempo a la hora UTC y tener en cuanta que el tiempo de compilación la da un retardo de unos 10s
void AjdjustTime() {
    int tYear = 2000;     //Los datos de las variables corresponden a las 24:00:00 UTC del 1/1/2000
    int tMonth = 1;       //Para poner en hora el reloj-calendario, cambiar el valor de las variables
    int tDay = 1;         //y compilar y cargar el sktech para que el RTC tome y actualice los datos
    int tHour = 24;       //Es necesario tener en cuenta que el tiempo de compilación y carga puede
    int tMinute = 0;      //variar entre 8 y 12 segundos.
    int tSecond = 0;      //Una vez cargados los nuevo datos volver a marcar la llamada a la función

    rtc.adjust(DateTime(tYear, tMonth, tDay, tHour, tMinute, tSecond)); //24:00:00 del 1 de enero de 2000
}

/*************************************************/
/**********    FUNCIONES DE PANTALLA    **********/
/*************************************************/

//Muestra, durante los segundos definidos en la variable, un mensaje de presentación
void InitialScreen() {
    boolean initiatedScreen = false;
    int timeScreen = 4;   //Segundos
    String txtOne = "MEDIDOR DE ROE";
    String txtTwo = " EA2HW, marzo 2019";

    do {
        chrono.tick();

        if (initiatedScreen == false) {
            lcd.clear();
            PrintLine(0, '*');

            lcd.setCursor(2, 1);
            lcd.print(txtOne);

            lcd.setCursor(0, 2);
            lcd.print(txtTwo);

            PrintLine(3, '*');
            initiatedScreen = true;
        }
    } while (chrono.havePassed(timeScreen) == false);
}

//Saca al display los datos de fecha y hora inciales en la pantalla
void InitialDate() {
    lcd.clear();
    DateTime n = rtc.now();
    dayInitial = n.day();
    DisplayDate(n);
    DisplayHour(n);
    RfMask(screenRX);
}

//Refresca los datos (data en formato float) en la pantalla
void DisplayData(float data, int pos, int line, int width, int decimals) {
    char c[width + 1];

    lcd.setCursor(pos, line);
    lcd.print(dtostrf(data, width, decimals, c));
}

//Crea una cadena con la fecha formateada, la almacena en un buffer y la saca al display en linea 0
void DisplayDate(DateTime t) {
    char buf_fecha[21];
    snprintf(buf_fecha, sizeof(buf_fecha), "%02d-%s-%4d (%s)", //Formato del buffer
             t.day(),
             TextMonth(t.month()).c_str(),
             t.year(),
             DayOfWeek(t.dayOfTheWeek()).c_str()
    );

    PrintPosition(0, 3, buf_fecha); //Saca al display centrando el texto (posición 3 = Centro)
}

//Crea una cadena con la hora formateada, la almacena en un buffer y la saca al Display en linea 1
void DisplayHour(DateTime t) {
    int diferenciaUTC = 1;

    char buf_hora[21];
    snprintf(buf_hora, sizeof(buf_hora), "%02d:%02d:%02d (%02d:%02d UTC)", //Formato del buffer
             t.hour() + diferenciaUTC,
             t.minute(),
             t.second(),
             t.hour(),
             t.minute()
    );
    lcd.setCursor(0, 1);
    lcd.print(buf_hora);
}

//Saca a display una máscara para los datos del medidor de ROE y potencia
void RfMask(int type) {
    String idCall = "EA2HW";
    String idText = "Esperando TX";

    lcd.setCursor(0, 2);
    if (type == screenRX) {
        PrintPosition(2, 3, idCall);
    } else {
        lcd.print("ROE=1:     POT=   W ");
    }

    lcd.setCursor(0, 3);
    if (type == screenRX) {
        PrintPosition(3, 3, idText);
    } else {
        lcd.print("ROE                 ");
    }
}

void PrintBar(float swr) {
    float posMax = 19.0;
    float PosInitial = 3.0;
    float PosBars = 5.0;
    int numLine = 3;
    float swrNull = 1.0;
    float swrMax = 4.0;

    int totBars = (posMax - PosInitial + 1) * PosBars;
    float valBar = (swrMax - swrNull) / totBars;
    float numBars =
    int((swr
    -swrNull) / valBar);

    for (int p = PosInitial; p <= posMax; p++) {
        lcd.setCursor(p, numLine);
        if (numBars == 0) {
            lcd.print(' ');
        } else {
            if (numBars >= PosBars) {
                lcd.write(PosBars);
                numBars -= PosBars;
            } else {
                lcd.write(numBars);
                numBars = 0;
            }
        }
    }
}

/************************************************************/
/**********    FUNCIONES AUXILIARES DE PANTALLA    **********/
/************************************************************/

//Saca al display un texto justificado
void PrintPosition(int line, int mode, String text) {  //modo = 1: Izquierda, 2: Derecha, 3: Centro
    int initPosition = 0;
    int maxWidth = 20;
    int textWidth = text.length();
    int textOver = maxWidth - textWidth;

    switch (mode) {           //Selecciona la posición de inicio
        case 1:
            initPosition = 0;
            break;
        case 2:
            initPosition = textOver;
            break;
        case 3:
            initPosition = textOver / 2;
            break;
    }

    lcd.setCursor(0, line);
    lcd.print("                    ");
    lcd.setCursor(initPosition, line);
    lcd.print(text);
}

//Saca a display una linea completa con un caracter
void PrintLine(int fila, char c) {  //Escribe en el display una línea con un caracter
    for (int i = 0; i <= 19; i++) {
        lcd.setCursor(i, fila);
        lcd.write(c);
    }
}

//Convierte el número de mes en cadena
String TextMonth(uint8_t mes) {
    switch (mes) {
        case 1:
            return "ene";
            break;
        case 2:
            return "feb";
            break;
        case 3:
            return "mar";
            break;
        case 4:
            return "abr";
            break;
        case 5:
            return "may";
            break;
        case 6:
            return "jun";
            break;
        case 7:
            return "jul";
            break;
        case 8:
            return "ago";
            break;
        case 9:
            return "sep";
            break;
        case 10:
            return "oct";
            break;
        case 11:
            return "nov";
            break;
        case 12:
            return "dic";
            break;
    }
}

//Convierte el número del día de la semana en cadena
String DayOfWeek(int dia) {
    switch (dia) {
        case 0:
            return "dom";
            break;
        case 1:
            return "lun";
            break;
        case 2:
            return "mar";
            break;
        case 3:
            return "mie";
            break;
        case 4:
            return "jue";
            break;
        case 5:
            return "vie";
            break;
        case 6:
            return "sab";
            break;
    }
}

void setup() {
    Serial.begin(9600);
    Serial.flush();

    //Inicia la clase lcd
    lcd.init();
    lcd.backlight();

    //Inicia la clase rtc
    rtc.begin();

    //Inicia la clase chrono
    chrono = Chronometer();

    //Desmarcar y actualizar los datos de las variables de la función para poner en hora el RTC
    //AdjustTime();

    //Crea los caracteres especiales de la barra con el número de barras verticales correspondientes
    lcd.createChar(1, b_one);
    lcd.createChar(2, b_two);
    lcd.createChar(3, b_three);
    lcd.createChar(4, b_four);
    lcd.createChar(5, b_five);

    InitialScreen();
    InitialDate();
}

void loop() {
    chrono.tick();
    int intervalTx = 0.5;
    int intervalRx = 4;

    if (chrono.havePassed(intervalTx)) {
        int rawForward = ReadSamples(pinDIR);  //Lee los saltos digitakes de la tensión en A0
        int rawReverse = ReadSamples(pinREF);  //Lee los saltos digitales de la tensión en A1

        if (rawForward > 0) {
            chrono.reset();

            float vForward = CalcVolts(rawForward);               //Tensión directa T2
            float vReverse = CalcVolts(rawReverse);               //Tensión reflejada en T1
            float valSwr = CalcSwr(vForward,
                                   vReverse);             //Calcula la ROE = (vForward + vReverse) / (vForward - vReverse)
            float wattsPower = CalcPower(vForward, vReverse);    //Calcula potencia = (vForward + vSwr)^2/50

            if (screenType == screenRX) {
                RfMask(screenTX);
                screenType = screenTX;
            }

            DisplayData(valSwr, 6, 2, 4, 2);      //Col=6, linea=2, ancho=3, decimales=2
            DisplayData(wattsPower, 15, 2, 3, 0); //Col=16, linea=, ancho=3, decimalews=0

            PrintBar(valSwr);
        }

        DateTime ahora = rtc.now();       //Lee los datos del RTC
        if (ahora.day() != dayInitial) {
            dayInitial = ahora.day();
            DisplayDate(ahora);
        }
        DisplayHour(ahora);
    }

    //Si han pasado 10 seconds sin transmitir, borra datos lecrtura de TX de las lineas del display
    if (chrono.havePassed(intervalRx)) {
        chrono.reset();
        RfMask(screenRX);
        screenType = screenRX;
    }
}
