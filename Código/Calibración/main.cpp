#include <Arduino.h>
#include <Servo.h>
#include <AccelStepper.h>


unsigned long t_ultimo_print = 0;
const int intervalo_print = 250;
int tiempo = 0;

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
  {A0, 512, 50, 973, 0, 0, 5, 50, 3, 0, 25, false}, //alto torque 0 centro 503
  {A1, 512, 50, 973, 0, 0, 5, 50, 3, 0, 25, false}, //1 codo sg90 centro 518
  {A2, 512, 50, 973, 0, 0, 5, 50, 3, 0, 25, false}, //2 centro 506
  {A3, 512, 50, 973, 0, 0, 5, 50, 3, 0, 25, false} //3 centro 518
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


S servos {Servo(), 8, 5, 175, 90, &joy[1], 85, 90, 90, 0.025, 0.0, 0.0}; 

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
    joy[s].j_rango = joy[s].j_rango * 0.90; //posible solucion ? 
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
  servos.servo.attach(servos.s_pin);
  servos.s_posicion_a = servos.s_centro; 
  servos.servo.write(90); 
}

//se encarga de calcular la posicion objetivo de los servomotores 

int mover(float j_valor){

  // si el joystick esta dentro de la zona muerta 
  if (fabs(j_valor) == 0.0){
    servos.s_posicion_o = servos.s_posicion_a; // posicion obj = posicion actual, no movimiento 
    servos.s_obj_grados = 0.0; // grados a mover = 0 
    return servos.s_posicion_o; // mantiene la posicion actual 
  }

  servos.s_obj_grados += fabs(j_valor); // determina la cantidad de grados objetivos a mover, PROBAR MULTIPLICANDO POR 0.75, ETC.

  //si hay mas de un grado para mover 
  if (servos.s_obj_grados >= 1.0){
 
    int pasos_obj = (int)servos.s_obj_grados; //transforma s_obj_grados a una variable entera, le saca la parte entera
    servos.s_obj_grados -= pasos_obj;  //deja en s_obj_grados solo la parte decimal para aprovecharla en la sig iteracion 

    int nueva_pos;

    //si se mueve para la derecha 
    if (j_valor > 0){
      nueva_pos = servos.s_posicion_o + pasos_obj; // determina la nueva posicion objetivo
    } 
    //si se mueve para la izquierda 
    else {
      nueva_pos = servos.s_posicion_o - pasos_obj;
    }

    //limita el nuevo valor de posicion a los maximos y minimos preestablecidos del servo 
    nueva_pos = constrain(nueva_pos, servos.s_min, servos.s_max);
    //limita la nueva posicion a valores cercanos a la posicion actual para evitar movimientos bruscos 
    nueva_pos = constrain(nueva_pos, servos.s_posicion_a - 5, servos.s_posicion_a + 5);

    servos.s_posicion_o = nueva_pos;
  }

  return servos.s_posicion_o; //devuelve la posicion objetivo 

}

// realiza el movimiento de los motores, transforma la pos objetivo en la actual 

void actualizacion(){

  int diff = servos.s_posicion_o - servos.s_posicion_a; //diferencia entre el obj y la actual  

  //si hay 1 grado o menos para mover 
  if (abs(diff) <= 1){
    servos.s_mov_grados = 0.0; //no devuelve movimiento 
    return; //pasa al siguiente motor 
  }

  //si hay movimiento, lo acumula como el ultimo movimiento + velocidad
  servos.s_mov_grados += servos.s_vel;

  // solo mueve cuando acumuló al menos 1 grado
  if (servos.s_mov_grados >= 1.0){

    int pasos = (int)servos.s_mov_grados;//guarda solo la parte entera del movimiento 
    servos.s_mov_grados -= pasos;// guarda la parte decimal del movimiento para futuras iteraciones 

    pasos = min(pasos, abs(diff));//limita el paso a 5 grados 
      
    //determina para que lado se mueve y limita el movimiento 
    servos.s_posicion_a += (diff > 0) ? pasos : -pasos; //si la resta es positiva se mueve para la derecha, pos_ac = pos_ac + pasos, analogo izq
    servos.s_posicion_a = constrain(servos.s_posicion_a, servos.s_min, servos.s_max); //limita la nueva posicion del servo para que no salga de los limites 
    servos.servo.write(servos.s_posicion_a); //realiza el movimiento del servo 
  }
}
     

void setup() {

  Serial.begin(115200); 
  calibrar(); //calibra los centros de los joysticks
  for (int i = 0; i<j_n; i++){
    Serial.println(joy[i].j_pin);
    Serial.println(joy[i].j_centro);  
    Serial.println(joy[i].j_rango);
    Serial.println(joy[i].j_min);
    Serial.println(joy[i].j_max);
    Serial.println("");
  }

  inicio(); //inicia los servomotores 
}

void loop() {

  float valor = j_val(servos.joy_ref); //determina el valor del joystick
  
    //si el joystick se movio
  if (fabs(valor) > 0.0){
    mover(valor); // determina la posicion objetivo
  }

  actualizacion(); //actualiza la posicion de los servomotores 
  if(millis() - t_ultimo_print >= intervalo_print){

    t_ultimo_print = millis(); 
    tiempo += intervalo_print; 
    Serial.println(tiempo);
    Serial.println(analogRead(joy[1].j_pin));
    Serial.println("valor"); 
    Serial.println(valor);
    Serial.println("posicion objetivo");
    Serial.println(servos.s_posicion_o);
    Serial.println("posicion actual");
    Serial.println(servos.s_posicion_a);
    Serial.println(""); 
  }
}
 
    