#include <Arduino.h>
#include <Servo.h> //librería para controlar los servos
#include <AccelStepper.h> //librería para controlar el motor paso a paso 

//define la estructura de los servos, con todas las variables de cada uno
struct S{
  Servo servo; //objeto
  int s_pin; //pin del servo
  int j_pin; //pin del joystick que lo controla
  int s_min; //angulo minimo
  int s_centro; //angulo central
  int s_max; //angulo maximo 
  float s_suav; //promedio de todas las posiciones 
  int j_centro; //val central del joystick
  unsigned long js_ultlectura; //hace cuanto fue la ultima lectura de los joy
};

const int s_n = 4; //cantidad de servomotores
const int j_medidas = 50; //cantidad de medidas para calibrar el centro 
const int  j_lecturas = 3; //lecturas del joystick 
const int j_zona = 60; //zona muerta joystick
const float suav = 0.70; //indice de suavizado, controla la "velocidad de respuesta"
bool calibrado = false; //por default sin calibrar

//crea los datos para cada servo 
S servos[s_n] ={
  {Servo(), 9, A0, 90, 90, 180, 90.0, 512, 0}, //garra del servo, pin 9, VRy pin A0 joystick 1 
  {Servo(), 10, A1, 0, 90, 180, 90.0, 512, 0}, //mano del servo, pin 10, VRx pin A1 joystick 1
  {Servo(), 11, A2, 0, 90, 180, 90.0, 512, 0}, //codo del servo, pin 11, VRy pin A2 joystick 2 
  {Servo(), 12, A2, 0, 90, 180, 90.0, 512, 0} //hombro del servo, pin 12, VRy pin A2 joystick 2 
};

//paso a paso
const int p_enable = 8; //habilitar y deshabilitar el motor 
const int p_dir = 7; //determina para que lado se mueve el motor
const int p_step = 6; //permite realizar los pasos
const int p_ms1 = 5; //determinan la cantidad de micro pasos por paso completo 
const int p_ms2 = 4; //determinan la cantidad de micro pasos por paso completo 
const int p_ms3 = 3; //determinan la cantidad de micro pasos por paso completo 

const int jp_pin = A3; //pin al que esta conectado el joystick del paso a paso
int jp_centro = 512; // determina un centro inicial para el joystick asociado al paso a paso 

const long p_vmax = 4000; //velocidad maxima
long p_pos = 0; 

AccelStepper stepper(1, p_step, p_dir); //crea el objeto paso a paso, especifica el tipo de driver (HR4988), pines a los cuales estan conectados step y dir. 
//especifica estos pines porque para controlar el paso a paso con el driver son necesarios esos pines. 

int p_jvalor = 0; //lectura del joystick para el paso a paso. 
long p_vel = 0; //velocidad incial 
unsigned long j_ultlectura = 0; //tiempo desde la ultima lectura
bool p_habilitado = true; //determina si esta prendido o no el motor 

//configuracion inicial del paso a paso. 
void p_config(){

  //Pines y funcion como salida 
  pinMode(p_ms1, OUTPUT);
  pinMode(p_ms2, OUTPUT);
  pinMode(p_ms3, OUTPUT);
  pinMode(p_enable, OUTPUT);

  //establece los micropasos como 1/4, 800 micropasos una vuelta. 
  digitalWrite(p_ms1, LOW);
  digitalWrite(p_ms2, HIGH);
  digitalWrite(p_ms3, LOW);

  digitalWrite(p_enable, LOW); //prende el motor. 
  p_habilitado = true; //avisa que esta prendido 

  stepper.setMaxSpeed(p_vmax); //establece una velocidad maxima 
  stepper.setAcceleration(2000); //establece una aceleracion maxima 

}

//funcion para calibrar el centro de los servos
void calibrar(){

  //repite el proceso para cada servo
  for (int s = 0; s < s_n; s++){
    long sum = 0; //reinicia el valor de sum

    //realiza 50 lecturas del valor del joystick cuando los servos estan quietos
    for (int i = 0; i < j_medidas; i++){
      sum += analogRead(servos[s].j_pin); //lecturas
      delay(10);
    }

    servos[s].j_centro = sum / j_medidas; //redefine el centro del joystick como el promedio

    //si un joystick esta muy desfasado le impone un centro 
    if (servos[s].j_centro < 485 || servos[s].j_centro > 539){
      servos[s].j_centro = 512; //cambiar para que el centro que le imponga sea el promedio de los otros centros
    }

  }

  long sum = 0; //reinicia el valor de sum

  //realiza las 50 lecturas del joy para el paso a paso 
  for(int a = 0; a < j_medidas; a++){

    sum += analogRead(jp_pin);
    delay(10);

  }

  jp_centro = sum / j_medidas; //calcula el centro del joy asociado al paso a paso 

  //si el centro esta muy desfasado le impone un centro
  if (jp_centro < 485 || jp_centro > 539){

    jp_centro = 512;

  } 

  calibrado = true; //avisa que esta calibrado

}

//elimina el ruido de los valores del joystick 
int j_val(int j_pin){ 

  long sum = 0; 

  //lee el joystick 3 veces
  for (int i = 0; i < j_lecturas; i++){
    sum += analogRead(j_pin);
    delay(1);
  }

  //le da al int el valor del promedio de las 3 lecturas
  return sum / j_lecturas; 
}

//elimina el ruido de los valores del joystick asociado al paso a paso. 
int jp_val(){

  long sum = 0;

  //lee el joystick 3 veces 
  for (int a = 0; a < j_lecturas; a++){
    sum += analogRead(jp_pin); 
    delay(1);
  }

  //le da el valor del promedio de las lecturas 
  return sum / j_lecturas;
}


//define la curva de movimiento del servo segun el movimiento del joystick
float curva (float input /* crea un float para reasignarlo */){
  return pow(input , 1.2); //le da al float el resultado de input elevado a 1.2 
} //baja la sensibilidad cerca del centro y la aumenta en los extremos 

float curva_p (float input /* crea un float para reasignarlo */){
  return pow(input , 1.6); //le da al float el resultado de input elevado a 1.2 
} //baja la sensibilidad cerca del centro y la aumenta en los extremos 

//define la posicion de los servomotores
int s_posicion(int s_n , int j_val/* crea un int para la cantidad de servos y otro para los valores del joystick */) {

  if(millis() - servos[s_n].js_ultlectura >= j_lecturas){
    servos[s_n].js_ultlectura = millis();
    j_val = analogRead(servos[s_n].j_pin);
  }

  int diff = j_val - servos[s_n].j_centro; //diferencia entre los valores del joystick y el centro de estos segun el servo

  if (abs(diff) <= j_zona){ 
    return servos[s_n].s_centro; //si el valor absoluto de la diferencia es menor o igual que la zona muerta mantiene el servo en el centro
  }

  float signo = (diff > 0) ? 1.00 : -1.0; //si diff es mayor que 0 le asigna 1.0 si es menor le asigna -1.0 
  float diff_a = abs(diff) - j_zona; //diferencia entre el valor absoluto de diff y la zona muerta para saber cuanto se movio fuera de esa zona. 

  float j_r1 = 1023.0 - j_zona - servos[s_n].j_centro; //rango maximo superior del joystick segun el centro calibrado, la zona muerta y el valor maximo
  float j_r2 = servos[s_n].j_centro - j_zona; //rango maximo inferior del joystick segun el centro calibrado y la zona muerta 
  float j_rango = min(j_r1, j_r2); //toma el valor minimo de los rangos maximos para evitar un mayor rango de movimiento hacia un lado

  float fraccion = diff_a / j_rango; //convierte el movimiento afuera de la zona muerta a un valor entre 0 y 1, 0 es rango minimo y 1 es rango maximo 
  fraccion = constrain(fraccion, 0.0, 1.0); //limita los valores de la fraccion para evitar que se pase del rango de movimiento
  fraccion = curva(fraccion); //crea la curva suave por medio de la funcion curva()

  int s_rango; //rango de movimiento desde el centro hacia un extremo del servo
  int posicion; //posicion final

  if (signo > 0){
    //movimiento entre 90° y 180°, desde el centro del joystick hasta 1023
    s_rango = servos[s_n].s_max - servos[s_n].s_centro; //diferencia entre el angulo maximo del servo y el centro
    posicion = servos[s_n].s_centro + (int)(fraccion * s_rango /* transforma la fraccion a unidades del servo */); //determina cuanto se mueve a partir del centro  
  } 
  else{
    //movimiento entre 0° y 90°, desde 0 hasta el centro del joystick 
    s_rango = servos[s_n].s_centro - servos[s_n].s_min; //diferencia entre el centro y el angulo minimo del servo
    posicion = servos[s_n].s_centro - (int)(fraccion * s_rango /* transforma la fraccion a unidades del servo */ ); //determina cuanto se mueve a partir del centro
  }

  return constrain(posicion, servos[s_n].s_min, servos[s_n].s_max); //le asigna al int el valor de posicion y lo limita entre el min y el max del servo
}

//define el movimiento suavizado del servo
void mover(int s_n, int posicion){
  servos[s_n].s_suav = servos[s_n].s_suav * suav /* determina el porcentaje del movimiento que va a ser determinado por el "historial" / + posicion * (1.0 - suav) / completa el movimiento con la nueva posicion */; 
  servos[s_n].servo.write((int)/* transforma el float a int */servos[s_n].s_suav); //le ordena al servo que se mueva a la posicion suave
}

void p_control(){

  if (!p_habilitado) return;
  digitalWrite(p_enable, LOW);
  p_habilitado = true;

  stepper.setMaxSpeed(p_vmax);
  stepper.setAcceleration(2000);

  stepper.setCurrentPosition(0);  
  p_pos = 0;

  p_jvalor = analogRead(jp_pin);
  int p_diff = p_jvalor - jp_centro; 

  if (abs(p_diff) <= j_zona){
    long pos_ac = stepper.currentPosition();
    stepper.stop(); 
    stepper.moveTo(pos_ac);
    p_pos = pos_ac;
    return;
   }

  float p_rango = 512 - j_zona; 
    
  float p_signo = (p_diff > 0) ? 1.0 : -1.0;
  float p_diff_a = abs(p_diff) -  j_zona;
  float p_fraccion = constrain (p_diff_a / p_rango, 0.0, 1.0); 
  //p_fraccion = curva(p_fraccion); 

  long t = (long)(p_fraccion * 800);
  p_pos = (p_diff > 0) ?  t : -t;    
  p_pos = constrain(p_pos, -800, 800);

  stepper.moveTo(p_pos);
  stepper.run();



}

void p_seguridad(bool habilitar = true){

  if (habilitar){

    if (fabs(stepper.speed()) < 8 && stepper.distanceToGo() != 0){

      digitalWrite(p_enable, HIGH);
      p_habilitado = false; 

    }
    else{

      digitalWrite(p_enable, LOW);  
      p_habilitado = true;

    }

  }

}


void setup() {
  //inicia todos los servos
  for (int i = 0; i < s_n; i++){
    servos[i].servo.attach(servos[i].s_pin); //pines a los que estan conectados los servos 
    servos[i].servo.write(servos[i].s_centro); //lleva a todos los servos al centro 
  }
  p_config();

}

void loop() {
  
  //revisa si esta calibrado
  if (!calibrado /* si es falso */){
    calibrar(); //calibra
    return; //reinicia el loop
  }

  //movimiento de los servos uno por uno 
  for(int i = 0; i < s_n; i++){
    int j_real = j_val(servos[i].j_pin/* define la int j_val a partir de las lecturas de los pines del joystick */); //determina el valor real del joystick
    int posicion = s_posicion(i, j_real); //determina la posicion a partir de los valores reales del joystick
    mover(i, posicion); //mueve el servo de manera suavizada a la posicion indicada
    //repite 4 veces, 1 vez por joystick
  }

  p_control();
  p_seguridad();
}