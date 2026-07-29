#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ==========================================
// AJUSTE DE VELOCIDAD
// ==========================================
const int VELOCIDAD = 255; // 0 (quieto) a 255 (máximo)

// ==========================================
// MAPA DE PINES (ESP32)
// ==========================================
const int IN1 = 26; 
const int IN2 = 27; 
const int IN3 = 33; 
const int IN4 = 25; 

const int pinBuzzer = 5;  
const int pinBoton = 15;  

// Pines nuevos para el Sensor Ultrasonido
const int pinTRIG = 18;
const int pinECHO = 19;

// Variable para saber qué está haciendo el robot
char estadoActual = 'S'; 

// ==========================================
// CONFIGURACIÓN DEL BLUETOOTH BLE
// ==========================================
#define SERVICE_UUID           "19b10000-e8f2-537e-4f6c-d104768a1214"
#define CHARACTERISTIC_UUID    "19b10001-e8f2-537e-4f6c-d104768a1214"

BLECharacteristic *pCharacteristic;
bool dispositivoConectado = false;

// ==========================================
// FUNCIONES DE MOVIMIENTO
// ==========================================
// ==========================================
// FUNCIONES DE MOVIMIENTO (CORREGIDAS)
// ==========================================
void avanzar() {
  analogWrite(IN1, VELOCIDAD); analogWrite(IN2, 0);
  analogWrite(IN3, VELOCIDAD); analogWrite(IN4, 0);
}

void retroceder() {
  analogWrite(IN1, 0); analogWrite(IN2, VELOCIDAD);
  analogWrite(IN3, 0); analogWrite(IN4, VELOCIDAD);
}

void izquierda() {
  analogWrite(IN1, 0); analogWrite(IN2, VELOCIDAD); 
  analogWrite(IN3, VELOCIDAD); analogWrite(IN4, 0); 
}

void derecha() {
  analogWrite(IN1, VELOCIDAD); analogWrite(IN2, 0); 
  analogWrite(IN3, 0); analogWrite(IN4, VELOCIDAD); 
}

void detener() {
  analogWrite(IN1, 0); analogWrite(IN2, 0);
  analogWrite(IN3, 0); analogWrite(IN4, 0);
}

void pitar() {
  digitalWrite(pinBuzzer, HIGH);
  delay(300); 
  digitalWrite(pinBuzzer, LOW);
}

// ==========================================
// FUNCIÓN MIDE DISTANCIA (ULTRASONIDO)
// ==========================================
long obtenerDistancia() {
  // Enviar un pulso limpio de 10 microsegundos
  digitalWrite(pinTRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(pinTRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinTRIG, LOW);
  
  // Medir cuánto tarda en regresar el eco (tiempo de espera máximo: 30ms)
  long duracion = pulseIn(pinECHO, HIGH, 30000);
  
  // Calcular la distancia en centímetros
  long distancia = (duracion * 0.0343) / 2;
  
  // Si da 0, significa que está fuera de rango (muy lejos o muy cerca)
  if (distancia == 0) return 999; 
  
  return distancia;
}

void ejecutarComando(char cmd) {
  estadoActual = cmd; // Guardamos lo que el usuario quiere hacer
  Serial.print("Comando: ");
  Serial.println(cmd);

  switch (cmd) {
    case 'A': avanzar(); break;
    case 'R': retroceder(); break;
    case 'I': izquierda(); break;
    case 'D': derecha(); break;
    case 'S': detener(); break;
    case 'P': pitar(); break;
  }
}

// ==========================================
// GESTIÓN DE CONEXIÓN BLE
// ==========================================
class MisEventosServidor: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      dispositivoConectado = true;
      Serial.println("¡Web conectada!");
    }
    void onDisconnect(BLEServer* pServer) {
      dispositivoConectado = false;
      Serial.println("Web desconectada.");
      detener();
      estadoActual = 'S';
      BLEDevice::startAdvertising(); 
    }
};

class MisEventosMensajes: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String valor = pCharacteristic->getValue().c_str();
      if (valor.length() > 0) {
        ejecutarComando(valor[0]); 
      }
    }
};

// ==========================================
// CONFIGURACIÓN INICIAL
// ==========================================
void setup() {
  Serial.begin(115200);

  // Configurar pines de motores y alertas
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);
  pinMode(pinBoton, INPUT_PULLUP);

  // Configurar pines del Ultrasonido
  pinMode(pinTRIG, OUTPUT);
  pinMode(pinECHO, INPUT);

  detener(); 

  // Iniciar BLE
  BLEDevice::init("RanaRobot"); 
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MisEventosServidor());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY 
                    );
  pCharacteristic->setCallbacks(new MisEventosMensajes());
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("¡RanaRobot lista con ojos ultrasónicos activos!");
}

// ==========================================
// BUCLE PRINCIPAL (SUPERVISIÓN)
// ==========================================
void loop() {
  // Si el robot está avanzando ('A'), vigilamos el camino
  if (estadoActual == 'A') {
    long cm = obtenerDistancia();
    
    // Si hay una pared u objeto a menos de 20 cm
    if (cm < 20) {
      detener();         // ¡Freno de mano!
      estadoActual = 'S'; // Cambiamos el estado a Detenido
      Serial.print("¡FRENADO DE EMERGENCIA! Obstáculo a: ");
      Serial.print(cm);
      Serial.println(" cm");
      
      // Alerta sonora
      pitar();
      delay(100);
      pitar();
    }
  }
  
  delay(60); // Breve pausa para no saturar el sensor y mantener vivo al MH-CD42
}
