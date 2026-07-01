#include <Arduino.h>
#include <Servo.h>
#include <AccelStepper.h>

//seteo joysticks

struct J{
  int j_pin; 
  float j_centro; 
  float j_min;
  float j_max; 
  float j_rango; //distancia del centro hasta un extremo (la menor)
  unsigned long j_lectura; //tiempo que paso desde la ultima lectura 
  const int j_tlectura; //tiempo que espera entre cada lectura
  const int j_cmedidas; //lecturas del joystick para determinar su centro 
  const int j_medidas; //cantidad de veces que se lee el joystick cada vez 
  float j_valor; //valor del joystick
  const int j_zonam; //zona muerta
  bool calibrado; //dete  rmina si se calculo el centro 
};

const int j_n = 4; 

J joy[j_n]{
  {A0, 512, 50, 973, 0, 0, 5, 50, 3, 0, 25, false}, //alto torque 0 
  {A1, 512, 50, 973, 0, 0, 5, 50, 3, 0, 25, false}, //1 codo sg90
  {A2, 512, 50, 973, 0, 0, 5, 50, 3, 0, 25, false}, //2 
  {A3, 512, 50, 973, 0, 0, 5, 50, 3, 0, 25, false} //3 
};

//seteo servos

struct S{
  Servo servo; //crea el objeto Servo. 
  const int s_pin;
  const int s_min; 
  const int s_max; 
  const int s_centro; 
  J* joy_ref; //puntero que almacena los datos de los joysticks 
  const int s_rango; //distancia del centro a los extremos 
  int s_posicion_a; //posicion actual del servo 
  int s_posicion_o; //posicion objetivo del servo
  const float s_vel; //cantidad de grados que se mueve por segundo
  float s_mov_grados; //movimiento en grados 
  float s_obj_grados; //movimiento objetivo en grados 
};

const int s_n = 3; 

S servos[s_n]{
  {Servo(), 8, 5, 175, 90, &joy[0], 85, 90, 90, 0.05, 0.0, 0.0}, //0
  {Servo(), 9, 5, 175, 90, &joy[1], 85, 90, 90, 0.05, 0.0, 0.0}, //1
  {Servo(), 10, 5, 175, 90, &joy[2], 85, 90, 90, 0.1, 0.0, 0.0} //2
}; 

//seteo garra 

struct G{
  Servo servo; //crea el objeto Servo. 
  const int g_pin;
  const int g_min; 
  const int g_max; 
  int g_posicion_a; //posicion actual del servo 
  const float g_vel; //tiempo entre cada movimiento del servo 
  const int bot_a; //pin del boton para abrir
  const int bot_c; // pin del boton para cerrar
  unsigned long t_ultimo_paso; // medicion de tiempo 
};

G garra{Servo(), 11, 5, 175, 90, 30, 12, 13,0};

//seteo paso a paso 
      
struct P{
  AccelStepper stepper; //crea el objeto stepper 
  const int p_enable; //prender el motor 
  const int p_dir; //determina la dirección del motor, 6
  const int p_step; //determina los pasos del motor, 5 
  const int p_ms1; //micropasos asociados al motor
  const int p_ms2; 
  const int p_ms3;
  J* joy_ref; //puntero que almacena datos de los joysticks 
  const float pv_max; //velocidad maxima 
  const float rampa; 
  float v_ac; 
  bool p_habilitado; 
};

P paso{
  AccelStepper(AccelStepper::DRIVER, 5 /*Step*/, 6 /*Dir*/) /*define el motor*/, 7, 6, 5, 4, 3, 2, &joy[3], 200.0, 0.08, 0.0, false
};
      
//calibrar y lectura joysticks

void calibrar(){

  //realiza el bucle para cada valor de s, abarcando los 4 joysticks 
  for ( int s = 0; s < j_n; s++){
    long sum = 0; //reinicia el valor de sum 

    for (int i = 0; i < joy[s].j_cmedidas; i++){
      sum += analogRead(joy[s].j_pin); //lee el joystick correpondiente una cierta cantidad de veces y las suma 
      delay(10); 
    }

    joy[s].j_centro = sum / joy[s].j_cmedidas; //le asigna como centro el cociente entre la suma y la cantidad de veces que se midio
    
    joy[s].j_valor = joy[s].j_centro; //inicializa el valor del joystick como su centro 

    //luego de determinar zona muerta curva 
    joy[s].j_max = 1023.0 - joy[s].j_centro - joy[s].j_zonam; //recalcula el valor maximo 
    joy[s].j_min = joy[s].j_centro - joy[s].j_zonam; //recalcula el valor minimo 

    joy[s].j_rango = min(joy[s].j_max, joy[s].j_min); //toma como que el joystick tiene el menor rango de movimiento para evitar forzar un lado
    joy[s].j_rango = joy[s].j_rango * 0.90;
    joy[s].j_rango = max(joy[s].j_rango, 1.0); //evita que el rango sea 0 y eso rompa todo 

  }

}

float j_val (J* joy /*define la variable a partir del puntero*/) {

  //lee el joystick cada 20ms
  if (millis() /* tiempo desde que el arduino esta ejecutando codigo */ - joy->j_lectura >= joy->j_tlectura){
    joy->j_lectura = millis(); //le modifica el tiempo de la ultima lectura al joystick 
        
    long sum = 0; 

    //realiza 3 lecturas del joystick 
    for (int i = 0; i < joy->j_medidas; i++){
      sum += analogRead(joy->j_pin);
    }

    joy->j_valor = sum/joy->j_medidas; //le asigna como valor el promedio de las lecturas 
    }

  //aplica la zona muerta
  float diff = joy->j_valor - joy->j_centro; //diferencia entre el valor medido y el centro 
  float signo = (diff >= 0) ? 1.00 : -1.00; //si la diferencia es mayor que 0, le asigna 1 y si no -1 (positivo y negativo)
  float diff_a = fabs(diff) - joy->j_zonam; //cuanto "salio" el joystick de la zona muerta 

  //si el joystick se movio afuera de la zona muerta 
  if (diff_a > 0){ 
    float fraccion = diff_a / joy->j_rango; //determina el movimiento con un valor entre 0 y 1 (0 no mov, 1 se mueve todo el rango)
    fraccion = constrain(fraccion, 0.0, 1.0); //limita el valor para evitar errores 
    fraccion = pow(fraccion, 1.3); //eleva el resultado a 1.3, disminuye la sensibilidad cerca del centro 

    //70% de valor viejo, 30% de val nuevo. fraccion * signo direccion y movimiento hacia el lado. 
    return (fraccion * signo); //calculo del valor final

  }

  //no se movio afuera de la zona muerta 
  else{ 
    return 0.0;
  }
        
}

//inicio y movimiento servos 

void inicio(){

  //asigna los pines a cada uno de los motores y los mueve al centro 
  for (int i = 0; i < s_n; i++){
    servos[i].servo.attach(servos[i].s_pin);
    servos[i].s_posicion_a = servos[i].s_centro; 
    servos[i].servo.write(90); 
  }

  //garra
  garra.servo.attach(garra.g_pin); 
  garra.servo.write(garra.g_posicion_a);
  //pines de los botones de los joysticks, resistencia interna 
  pinMode(garra.bot_a, INPUT_PULLUP); 
  pinMode(garra.bot_c, INPUT_PULLUP);
}

//se encarga de calcular la posicion objetivo de los servomotores 

int mover(int h, float j_valor){

  // si el joystick esta dentro de la zona muerta 
  if (fabs(j_valor) == 0.0){
    servos[h].s_posicion_o = servos[h].s_posicion_a; // posicion obj = posicion actual, no movimiento 
    servos[h].s_obj_grados = 0.0; // grados a mover = 0 
    return servos[h].s_posicion_o; // mantiene la posicion actual 
  }

  servos[h].s_obj_grados += fabs(j_valor); // determina la cantidad de grados objetivos a mover, PROBAR MULTIPLICANDO POR 0.75, ETC.

  //si hay mas de un grado para mover 
  if (servos[h].s_obj_grados >= 1.0){
 
    int pasos_obj = (int)servos[h].s_obj_grados; //transforma s_obj_grados a una variable entera, le saca la parte entera
    servos[h].s_obj_grados -= pasos_obj;  //deja en s_obj_grados solo la parte decimal para aprovecharla en la sig iteracion 

    int nueva_pos;

    //si se mueve para la derecha 
    if (j_valor > 0){
      nueva_pos = servos[h].s_posicion_o + pasos_obj; // determina la nueva posicion objetivo
    } 
    //si se mueve para la izquierda 
    else {
      nueva_pos = servos[h].s_posicion_o - pasos_obj;
    }

    //limita el nuevo valor de posicion a los maximos y minimos preestablecidos del servo 
    nueva_pos = constrain(nueva_pos, servos[h].s_min, servos[h].s_max);
    //limita la nueva posicion a valores cercanos a la posicion actual para evitar movimientos bruscos 
    nueva_pos = constrain(nueva_pos, servos[h].s_posicion_a - 5, servos[h].s_posicion_a + 5);

    servos[h].s_posicion_o = nueva_pos;
  }

  return servos[h].s_posicion_o; //devuelve la posicion objetivo 

}

// realiza el movimiento de los motores, transforma la pos objetivo en la actual 

void actualizacion(int i){

  int diff = servos[i].s_posicion_o - servos[i].s_posicion_a; //diferencia entre el obj y la actual  

  //si hay 1 grado o menos para mover 
  if (abs(diff) <= 1){
    servos[i].s_mov_grados = 0.0; //no devuelve movimiento 
    return; //pasa al siguiente motor 
  }

  //si hay movimiento, lo acumula como el ultimo movimiento + velocidad
  servos[i].s_mov_grados += servos[i].s_vel;

  // solo mueve cuando acumuló al menos 1 grado
  if (servos[i].s_mov_grados >= 1.0){

    int pasos = (int)servos[i].s_mov_grados;//guarda solo la parte entera del movimiento 
    servos[i].s_mov_grados -= pasos;// guarda la parte decimal del movimiento para futuras iteraciones 

    pasos = min(pasos, abs(diff));//limita el paso a 5 grados 
      
    //determina para que lado se mueve y limita el movimiento 
    servos[i].s_posicion_a += (diff > 0) ? pasos : -pasos; //si la resta es positiva se mueve para la derecha, pos_ac = pos_ac + pasos, analogo izq
    servos[i].s_posicion_a = constrain(servos[i].s_posicion_a, servos[i].s_min, servos[i].s_max); //limita la nueva posicion del servo para que no salga de los limites 
    servos[i].servo.write(servos[i].s_posicion_a); //realiza el movimiento del servo 
  }
}
     
//garra

void agarrar(){
  
  //si pasaron 50 ms o mas entre pasos 
  if (millis() - garra.t_ultimo_paso >= garra.g_vel){

    garra.t_ultimo_paso = millis(); 


    if ((digitalRead(garra.bot_a) == LOW) && (digitalRead(garra.bot_c) == LOW)){
      garra.g_posicion_a = 90; 
    }
    //si el boton de abrir se presiona se abre 5 grados
    else if (digitalRead(garra.bot_a) == LOW){
      garra.g_posicion_a += 5;
    }
    //si el boton de cerrar se presiona se cierra 5 grados 
    else if (digitalRead(garra.bot_c) == LOW){
      garra.g_posicion_a -= 5;
    }

    

    garra.g_posicion_a = constrain(garra.g_posicion_a, garra.g_min, garra.g_max);
    garra.servo.write(garra.g_posicion_a); //mueve la garra
  }

}

//configuracion y movimiento motor 

void p_config(){

  //tipo de pin de los micro pasos y enable 
  pinMode(paso.p_ms1, OUTPUT); 
  pinMode(paso.p_ms2, OUTPUT);
  pinMode(paso.p_ms3, OUTPUT);
  pinMode(paso.p_enable, OUTPUT);

  //setea los micropasos a 1/4, 800 una vuelta 
  digitalWrite(paso.p_ms1, LOW);
  digitalWrite(paso.p_ms2, HIGH); 
  digitalWrite(paso.p_ms3, LOW); 

  //inicia el motor
  digitalWrite(paso.p_enable, LOW);
  paso.p_habilitado = true; 

  //velocidades maximas y aceleración máxima 
  paso.stepper.setMaxSpeed(paso.pv_max); 

}

void p_mov(){

  if(!paso.p_habilitado) return; 
      
  float v = j_val(paso.joy_ref); //lee el joy asociado, val entre -1 y 1 
  float velocidad = v * paso.pv_max; //convierte el valor en una velocidad dentro del umbral
  paso.v_ac += (velocidad - paso.v_ac) * paso.rampa; //aumenta la velocidad de manera progresiva hasta llegar a la velocidad deseada 

  if (fabs(paso.v_ac) < 5.0) paso.v_ac = 0.0; //si la velocidad es muy baja lo detiene, evita movimiento por ruido

  paso.stepper.setSpeed(paso.v_ac);
  paso.stepper.runSpeed();

}

void setup() {

  calibrar(); //calibra los centros de los joysticks 
  inicio(); //inicia los servomotores 
  p_config(); //inicia el motor 
        
}

void loop() {

  //recorre cada uno de los joysticks asociados a los servos  
  for (int i = 0; i < s_n; i++){
          
    float valor = j_val(servos[i].joy_ref); //determina el valor del joystick 

    //si el joystick se movio
    if (fabs(valor) > 0.0){
      mover(i,valor); // determina la posicion objetivo
    }

    actualizacion(i); //actualiza la posicion de los servomotores 
  
  }   
  agarrar(); 
  p_mov(); //mueve el motor hacia alguno de los dos lados, movimiento por velocidad 
  
}
    