/*
 * Sketck para controlar un instrumento que monitoriza la potencia y ROE de un línea de transmisión
 * de radio en en el segmento de radioaficionados de HF (160 a 10 metros) con un transmisor de hasta
 * 200 vatios. Le descricción del proyecto, esquemas, diseño y explicación del código se han 
 * publicado en la revista RADIOFICIONADOS (URE) en los números de marso, abril y mayo de 2019)
 * Por Arsenio "Enio" Gutiérrez, EA2HW,
 * Software de uso libre bajo la responsabilidad del usuario. El autor no se hace responsables de 
 * cualquier daño cauado por su uso. v1 marzo de 2019
 */

#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#define pinDIR 0
#define pinREF 1

int diaInicial;
int pantTX = 0;
int pantRX = 1;
int tipoPantalla = pantRX;

byte b_uno[8] = {B10000, B10000, B10000, B10000, B10000, B10000, B10000};
byte b_dos[8] = {B11000, B11000, B11000, B11000, B11000, B11000, B11000};
byte b_tres[8] = {B11100, B11100, B11100, B11100, B11100, B11100, B11100};
byte b_cuatro[8] = {B11110, B11110, B11110, B11110, B11110, B11110, B11110};
byte b_cinco[8] = {B11111, B11111, B11111, B11111, B11111, B11111, B11111};

class Cronometro{
        public:
        Cronometro();
        void tick();
        void reset();
        boolean hanPasado(const int segundos);
        private:
        unsigned long _currentTick;
        unsigned long _lastTick;
};

Cronometro::Cronometro() {  //Inicia el cronómetro
    _lastTick = millis();
    _currentTick = millis();
};

void Cronometro::tick() {   //Captura los milisegundos iniciales
    _currentTick = millis();
};

void Cronometro::reset() {  //Iguala los milisegundos iniciales y actuales
    _lastTick = _currentTick;
};

boolean Cronometro::hanPasado(const int segundos) {     //Devuelve si han pasado los segundos indicados
    return _currentTick >= _lastTick + (segundos * 1000);
};

LiquidCrystal_I2C lcd(0x3f, 20, 4);
RTC_DS3231 rtc;
Cronometro crono;

void setup() {
    Serial.begin(9600);
    Serial.flush();

    //Inicia la clase lcd
    lcd.init();
    lcd.backlight();

    //Inicia la clase rtc
    rtc.begin();

    //Inicia la clase crono
    crono = Cronometro();

    //Actualiza el tiempo actual (fecha y hora)
    //Desmarcar la llamada a la función e introducir los datos de la fecha y hora actuales, compilar y subir
    //ActualizarTiempo()

    //Crea los caracteres especiales de la barra con el número de barras verticales correspondientes
    lcd.createChar(1, b_uno);
    lcd.createChar(2, b_dos);
    lcd.createChar(3, b_tres);
    lcd.createChar(4, b_cuatro);
    lcd.createChar(5, b_cinco);

    PantallaInicial();
    MascaraFechayHora();
    MascaraRF(pantRX);
}

void loop() {
    crono.tick();
    int pausaTX = 1;
    int pausaRX = 10;

    if (crono.hanPasado(pausaTX)) {
        int rawDIR = LeerMuestras(pinDIR);  //Lee los saltos digitakes de la tensión en A0
        int rawREF = LeerMuestras(pinREF);  //Lee los saltos digitales de la tensión en A1

        if (rawDIR > 0) {
            crono.reset();

            float vDIR = CalcVoltios(rawDIR);         //Tensión directa T2
            float vREF = CalcVoltios(rawREF);         //Tensión reflejada en T1
            float rROE = CalcROE(vDIR, vREF);         //Calcula la ROE = (vDIR + vREF) / (vDIR - vREF)
            float wTOT = CalcPotencia(vDIR + vREF);   //Calcula potencia = (vDIR + vROE)^2/50

            if (tipoPantalla == pantRX) {
                MascaraRF(pantTX);
                tipoPantalla = pantTX;
            }

            MostrarResultados(rROE, wTOT);
            SacarBarra(rROE);
        }

        DateTime ahora = rtc.now();       //Lee los datos del RTC
        if (ahora.day() != diaInicial) {
            diaInicial = ahora.day();
            MostrarFecha(ahora);
        }
        MostrarHora(ahora);
    }

    //Si han pasado 10 segundos sin transmitir, borra datos lecrtura de TX de las lineas del display
    if (crono.hanPasado(pausaRX)) {
        crono.reset();
        MascaraRF(pantRX);
        tipoPantalla = pantRX;
    }
}

/***************************************************/
/**********          SUBRUTINAS           **********/
/***************************************************/

//Toma (rawLecturas) muestras raw de los pines análogicos de lectura de tensión directa y reflejada y devuelve la mas alta
int LeerMuestras(int pinLectura) {  //Toma rawLecturas muestras en A0 (pinDIR) = Directa y A1 (pinREF) = Reflejada y devuelve ña lectura más alta
    int rawMax = 0;
    int rawLecturas = 1111;
    String entrada;

    for (int i = 0; i < rawLecturas; i++) {
        int raw = analogRead(pinLectura);

        if (raw > rawMax) {
            rawMax = raw;
        }
    }
    return rawMax;
}

//Calcula los Voltios rms de la RF directa y reflejada de la linea de transmision
float CalcVoltios(int raw) {
    float rUno = 4740.0;                        //Divisor de tensión R1
    float rDos = 10720.0;                       //Divisor de tensión R2
    float maxVolt = 5.0;                        //Máximo voltaje de entrada en los I/O analógicos
    float rawSaltos = 1023.0;                   //Saltos digitales correspondientes a los ADC de 10bit
    float diodo = 0.3;                          //Caída de tensión en el diodo 1N5711

    float razonTransformador = 1.0 / 23.0;        //Devanado de T1 y T2 = 1::23 espiras
    float rmsFactor = sqrt(
            2);                                            //Tensión de pico a tensión RMS = raiz cuadrada de 2
    float divTension = rDos / (rUno +
                               rDos);                              //Si R2 = 0, el divisor es 1, luego Vsal = Vent, Vsal = Vent*(R2/(R1+R2))
    float trsFactor = 20 * abs(log10(
            razonTransformador));               //Factor de ganancia de tensión en los transformadores T1 y T2
    float vSalida = (((raw * maxVolt) / rawSaltos) / divTension) + 0.3;   //Tensión a la salida del transformador

    return (vSalida * trsFactor) / rmsFactor; // Tensión RMS en la línea
}

float CalcROE(float vDir, float vRef) { //ROE = (vDirecta + vReflejada) / (vDirecta - vReflejada)
    return (vDir + vRef) / (vDir - vRef);
}

float CalcPotencia(float vRms) {        //Potencia = cuadrado de la tensión partido pr la impedancia de la antena
    float impAntena = 50.0;

    return pow(vRms, 2) / impAntena;
}

/***************************************************/
/**********     FUNCIONES DE PANTALLA     **********/
/***************************************************/

//Muestra durante pausaPresentacion segundos un mensaje de bienvenida
void PantallaInicial() {
    boolean mostrarPresentacion = false;
    int pausaPresentacion = 4;
    boolean haTerminado = false;

    do {
        crono.tick();
        haTerminado = crono.hanPasado(pausaPresentacion);

        if (mostrarPresentacion == false) {
            lcd.clear();
            lcd.setCursor(0, 0);
            EscribeLinea(0, '*');
            EscribePosicion(1, 3, "MEDIDOR DE ROE v1.0 ");
            EscribePosicion(2, 3, "EA2HW, marzo 2019");
            EscribeLinea(3, '*');
            mostrarPresentacion = true;
        }
    } while (haTerminado == false);
}

//Inicia la pantalla de fecha y hora
void MascaraFechayHora() {
    lcd.clear();
    DateTime ahora = rtc.now();
    diaInicial = ahora.day();
    MostrarFecha(ahora);
    MostrarHora(ahora);
}

//Saca al display los resultados de los cáculos de ROE y potencia
void MostrarResultados(float roe, float pot) {
    lcd.setCursor(6, 2);
    lcd.print(NumAcadena(roe, 3, 2, false));

    lcd.setCursor(16, 2);
    lcd.print(NumAcadena(pot, 3, 0, false));
}

//Crea una cadena con la fecha formateada, la almacena en un buffer y la saca al display en linea 0
void MostrarFecha(DateTime tiempo) {
    char buf_fecha[21];
    snprintf(buf_fecha, sizeof(buf_fecha), "%02d-%s-%4d (%s)", //Formato del buffer
             tiempo.day(),
             mesNombre(tiempo.month()).c_str(),
             tiempo.year(),
             diaSemanaNombre(tiempo.dayOfTheWeek()).c_str()
    );

    EscribePosicion(0, 3, buf_fecha); //Saca al display centrando el texto (posición 3 = Centro)
}

//Crea una cadena con la hora formateada, la almacena en un buffer y la saca al Display en linea 1
void MostrarHora(DateTime tiempo) {
    int diferenciaUTC = 1;

    char buf_hora[21];
    snprintf(buf_hora, sizeof(buf_hora), "%02d:%02d:%02d (%02d:%02d UTC)", //Formato del buffer
             tiempo.hour() + diferenciaUTC,
             tiempo.minute(),
             tiempo.second(),
             tiempo.hour(),
             tiempo.minute()
    );

    lcd.setCursor(0, 1); //Muestra la cadena de la hora en la línea 2 del display
    lcd.print(buf_hora);
}

//Saca a display una máscara para los datos del medidor de ROE y potencia
void MascaraRF(int tipo) {
    lcd.setCursor(0, 2);
    if (tipo == pantRX) {
        EscribePosicion(2, 3, "EA2HW");
    } else {
        lcd.print("ROE=1:     POT=    W");
    }

    lcd.setCursor(0, 3);
    if (tipo == pantRX) {
        EscribePosicion(3, 3, "Esperando TX");
    } else {
        lcd.print("ROE                 ");
    }
}

//Saca a pantalla una barra proporcional de la ROE (Entre 1:1 y 1:4)
void SacarBarra(float roe) {
    float posMaxima = 19.0;
    float posInicial = 3.0;
    float posBarras = 5.0;
    int numLinea = 3;
    float roeNula = 1.0;
    float roeMaxima = 4.0;

    int totBarras = (posMaxima - posInicial + 1) * posBarras;
    float valBarra = (roeMaxima - roeNula) / totBarras;
    float numBarras =
    int((roe
    -roeNula) / valBarra);

    for (int posicion = posInicial; posicion <= posMaxima; posicion++) {
        lcd.setCursor(posicion, numLinea);
        if (numBarras == 0) {
            lcd.print(' ');
        } else {
            if (numBarras >= posBarras) {
                lcd.write(posBarras);
                numBarras -= posBarras;
            } else {
                lcd.write(numBarras);
                numBarras = 0;
            }
        }
    }
}

/***************************************************/
/**********     FUNCIONES AUXILIARES      **********/
/***************************************************/

void ActualizarTiempo() {
    int anno = 2000;
    int mes = 1;
    int dia = 1;
    int diaSemana = 0;  //0=dom, 1=lunes, etc
    int hora = 24;
    int minuto = 0;
    int segundo = 0;
}

//Saca al display un texto justificado
void EscribePosicion(int fila, int modo, String texto) {  //modo = 1: Izquierda, 2: Derecha, 3: Centro
    int posicion = 0;
    int maxLargo = 20;
    int largo = texto.length();
    int sobra = maxLargo - largo;

    switch (modo) {           //Selecciona la posición de inicio
        case 1:
            posicion = 0;
            break;
        case 2:
            posicion = sobra;
            break;
        case 3:
            posicion = sobra / 2;
            break;
        default:
            delay(0);
    }

    lcd.setCursor(0, fila);
    lcd.print("                    ");
    lcd.setCursor(posicion, fila);
    lcd.print(texto);
}

//Saca a display una linea completa con un caracter
void EscribeLinea(int fila, char caracter) {  //Escribe en el display una línea con un caracter
    for (int i = 0; i <= 19; i++) {
        lcd.setCursor(i, fila);
        lcd.write(caracter);
    }
}

//Convierte el número de mes en cadena
String mesNombre(uint8_t mes) {
    switch (mes) {
        case 1:
            return "ene";
        case 2:
            return "feb";
        case 3:
            return "mar";
        case 4:
            return "abr";
        case 5:
            return "may";
        case 6:
            return "jun";
        case 7:
            return "jul";
        case 8:
            return "ago";
        case 9:
            return "sep";
        case 10:
            return "oct";
        case 11:
            return "nov";
        case 12:
            return "dic";
    }
}

//Convierte el número del día de la semana en cadena
String diaSemanaNombre(int dia) {
    switch (dia) {
        case 0:
            return "dom";
        case 1:
            return "lun";
        case 2:
            return "mar";
        case 3:
            return "mie";
        case 4:
            return "jue";
        case 5:
            return "vie";
        case 6:
            return "sab";
    }
}

//Crea una cadena justificada en extremos o centro patiendo de un numero float
String NumAcadena(float num, int largo, int dec, boolean ceros) {
    char c[1 + largo];
    String cadena;

    dtostrf(num, largo, dec, c);
    cadena = String(c);

    if (ceros) {
        cadena.replace(" ", "0");
    }
    return cadena;
}
