#include <LoRa.h>
#include <SPI.h>
#include <Wire.h>
#include <PubSubClient.h> // Importa a Biblioteca PubSubClient
#include <WiFi.h> // Importa a Biblioteca ESP8266WiFi


/* Definicoes para comunicação com radio LoRa */
#define SCK_LORA           5
#define MISO_LORA          19
#define MOSI_LORA          27
#define RESET_PIN_LORA     14
#define SS_PIN_LORA        18

#define HIGH_GAIN_LORA     14  /* dBm */
#define BAND               915E6  /* 915MHz de frequencia */

/* Definicoes gerais */
#define DEBUG_SERIAL_BAUDRATE    9600

#define TOPICO_PUBLISH   "tiago/sensores"    //tópico MQTT de envio de informações para Broker
#define ID_MQTT  "ColmeiaMsg"     //id mqtt (para identificação de sessão)
                               //IMPORTANTE: este deve ser único no broker (ou seja, 
                               //            se um client MQTT tentar entrar com o mesmo 
                               //            id de outro já conectado ao broker, o broker 
                               //            irá fechar a conexão de um deles).

// MQTT
const char* BROKER_MQTT = "broker.mqttdashboard.com"; //URL do broker MQTT que se deseja utilizar
int BROKER_PORT = 1883; // Porta do Broker MQTT

//Variáveis e objetos globais
WiFiClient espClient; // Cria o objeto espClient
PubSubClient MQTT(espClient); // Instancia o Cliente MQTT passando o objeto espClient

// WIFI
const char* SSID = "casa_ta3_EXT"; // SSID / nome da rede WI-FI que deseja se conectar
const char* PASSWORD = "senha123"; // Senha da rede WI-FI que deseja se conectar

/* Local prototypes */
bool init_comunicacao_lora(void);

void initWiFi();
void initMQTT();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void VerificaConexoesWiFIEMQTT(void);


/* Funcao: inicia comunicação com chip LoRa
 * Parametros: nenhum
 * Retorno: true: comunicacao ok
 *          false: falha na comunicacao
*/
bool init_comunicacao_lora(void)
{
    bool status_init = false;
    Serial.println("[LoRa Receiver] Tentando iniciar comunicacao com o radio LoRa...");
    SPI.begin(SCK_LORA, MISO_LORA, MOSI_LORA, SS_PIN_LORA);
    LoRa.setPins(SS_PIN_LORA, RESET_PIN_LORA, LORA_DEFAULT_DIO0_PIN);
    LoRa.receive();
    if (!LoRa.begin(BAND)) 
    {
        Serial.println("[LoRa Receiver] Comunicacao com o radio LoRa falhou. Nova tentativa em 1 segundo...");        
        delay(1000);
        status_init = false;
    }
    else
    {
        /* Configura o ganho do receptor LoRa para 20dBm, o maior ganho possível (visando maior alcance possível) */ 
        LoRa.setTxPower(HIGH_GAIN_LORA); 
        Serial.println("[LoRa Receiver] Comunicacao com o radio LoRa ok");
        status_init = true;
    }
    
    return status_init;
}

/* Funcao de setup */
void setup() 
{
    initWiFi();
    initMQTT();
    
    Serial.begin(DEBUG_SERIAL_BAUDRATE);
    while (!Serial);

    /* Tenta, até obter sucesso, comunicacao com o chip LoRa */
    while(init_comunicacao_lora() == false);   
    
}

/* Programa principal */
void loop() 
{
  //garante funcionamento das conexões WiFi e ao broker MQTT
    //VerificaConexoesWiFIEMQTT();
    
    char byte_recebido;
   
    int lora_rssi = 0;
    char informacao_recebida = 0;
    char * ptInformaraoRecebida = NULL;
    int i = 0;
    int j = 0;
    char dados[60];
    /* Verifica se chegou alguma informação do tamanho esperado */
    int packet_size = LoRa.parsePacket();
    yield();
    if (packet_size)  {
      
        Serial.println("[LoRa Receiver] Há dados a serem lidos");
        
        /* Recebe os dados conforme protocolo */               
        ptInformaraoRecebida = (char *)&informacao_recebida;  
        while (LoRa.available()) 
        {
            byte_recebido = (char)LoRa.read();
            //Serial.print(byte_recebido);
            *ptInformaraoRecebida = byte_recebido;
           // ptInformaraoRecebida++;informacao_recebida
           dados[i++] = byte_recebido;
            
        }
        VerificaConexoesWiFIEMQTT();
        Serial.println(dados);
        MQTT.publish(TOPICO_PUBLISH, dados);
        /* Escreve RSSI de recepção e informação recebida */
        lora_rssi = LoRa.packetRssi();
       
        MQTT.loop();
     
    }
    
    
}

 
//Função: inicializa e conecta-se na rede WI-FI desejada
//Parâmetros: nenhum
//Retorno: nenhum
void initWiFi()
{
    delay(10);
    Serial.println("------Conexao WI-FI------");
    Serial.print("Conectando-se na rede: ");
    Serial.println(SSID);
    Serial.println("Aguarde");
     
    reconectWiFi();
}


//Função: inicializa parâmetros de conexão MQTT(endereço do 
//        broker, porta e seta função de callback)
//Parâmetros: nenhum
//Retorno: nenhum
void initMQTT() 
{
    MQTT.setServer(BROKER_MQTT, BROKER_PORT);   //informa qual broker e porta deve ser conectado
}


//Função: reconecta-se ao broker MQTT (caso ainda não esteja conectado ou em caso de a conexão cair)
//        em caso de sucesso na conexão ou reconexão, o subscribe dos tópicos é refeito.
//Parâmetros: nenhum
//Retorno: nenhum
void reconnectMQTT() 
{
    while (!MQTT.connected()) 
    {
        Serial.print("* Tentando se conectar ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        if (MQTT.connect(ID_MQTT)) 
        {
            Serial.println("Conectado com sucesso ao broker MQTT!");
        } 
        else
        {
            Serial.println("Falha ao reconectar no broker.");
            Serial.println("Havera nova tentatica de conexao em 2s");
            delay(5000);
        }
    }
}

//Função: reconecta-se ao WiFi
//Parâmetros: nenhum
//Retorno: nenhum
void reconectWiFi() 
{
    //se já está conectado a rede WI-FI, nada é feito. 
    //Caso contrário, são efetuadas tentativas de conexão
    if (WiFi.status() == WL_CONNECTED)
        return;
         
    WiFi.begin(SSID, PASSWORD); // Conecta na rede WI-FI
     
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(1000);
        Serial.print(".");
    }
   
    Serial.println();
    Serial.print("Conectado com sucesso na rede ");
    Serial.print(SSID);
    Serial.println("IP obtido: ");
    Serial.println(WiFi.localIP());
}

//Função: verifica o estado das conexões WiFI e ao broker MQTT. 
//        Em caso de desconexão (qualquer uma das duas), a conexão
//        é refeita.
//Parâmetros: nenhum
//Retorno: nenhum
void VerificaConexoesWiFIEMQTT(void)
{
    if (!MQTT.connected()) 
        reconnectMQTT(); //se não há conexão com o Broker, a conexão é refeita
     
     reconectWiFi(); //se não há conexão com o WiFI, a conexão é refeita
}


  
