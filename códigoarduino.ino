#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>  // Corrigido para ESP8266
#include <WebSocketsServer.h>

// Definir credenciais do ponto de acesso WiFi
const char* ssid = "esp8266-Chat";
const char* password = "Zeus6996";

// Endereço IP e Gateway estáticos
IPAddress local_IP(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// Criar servidor web e websocket
AsyncWebServer server(80);
WebSocketsServer webSocket(81); // Porta WebSocket

String chatHistory = "";  // Histórico do chat
const size_t maxChatSize = 15 * 1024;  // Limite de 15KB para o chat
int usersOnline = 0; // Contador de usuários online

// HTML da página de chat
const char* htmlPage = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP8266 Chat</title>
  <style>
    body { font-family: Arial; display: flex; flex-direction: column; height: 100vh; margin: 0; }
    #nameSection, #chatSection { padding: 10px; }
    #chatBox { flex: 1; overflow-y: scroll; border: 1px solid #000; padding: 5px; margin-bottom: 10px; max-height: 70vh; }
    #chatSection { display: none; }
    input[type=text], button { padding: 10px; font-size: 16px; }
    #chatInput { width: 80%; }
  </style>
</head>
<body>
  <div id="nameSection">
    <h2>Insira seu nome:</h2>
    <input type="text" id="nameInput" placeholder="Seu nome">
    <button onclick="saveName()">Salvar</button>
  </div>
  <div id="chatSection">
    <div id="userCount">Users: 0</div> <!-- Contador de usuários -->
    <div id="chatBox"></div>
    <input type="text" id="chatInput" maxlength="255" placeholder="Digite sua mensagem (máximo 255 caracteres)">
    <button onclick="sendMessage()">Enviar</button>
  </div>

  <script>
    var userName = '';
    var webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');

    // Função para salvar o nome e liberar o chat
    function saveName() {
      var name = document.getElementById("nameInput").value;
      if (name) {
        userName = name;
        document.getElementById("nameSection").style.display = "none";
        document.getElementById("chatSection").style.display = "block";
      }
    }

    // Função para enviar a mensagem pelo WebSocket
    function sendMessage() {
      var message = document.getElementById("chatInput").value;
      if (message && userName) {
        webSocket.send(userName + ": " + message);
        document.getElementById("chatInput").value = '';
      }
    }

    // Recebe novas mensagens
    webSocket.onmessage = function(event) {
      var chatBox = document.getElementById("chatBox");
      var userCount = document.getElementById("userCount");
      
      // Verificar se a mensagem é a contagem de usuários
      if (event.data.startsWith("Users: ")) {
        userCount.innerHTML = event.data; // Atualizar o contador de usuários
      } else {
        chatBox.innerHTML += event.data + "<br>";
        chatBox.scrollTop = chatBox.scrollHeight; // Rolagem automática para a última mensagem
      }
    };
  </script>
</body>
</html>
)rawliteral";

// Função para limpar o histórico de mensagens quando atingir o tamanho máximo
void clearChatHistory() {
  chatHistory = "";  // Limpar o histórico
  Serial.println("Chat foi limpo automaticamente para liberar memória.");
}

// Função para lidar com novas conexões WebSocket
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    // Mensagem recebida de um cliente WebSocket
    String message = String((char *)payload);

    // Adicionar a mensagem ao histórico
    chatHistory += message + "<br>";

    // Verificar se o tamanho do histórico ultrapassou o limite
    if (chatHistory.length() > maxChatSize) {
      clearChatHistory();  // Limpar o histórico se ultrapassar o limite
    }

    // Enviar a mensagem para todos os clientes conectados
    webSocket.broadcastTXT(message);
  }
  else if (type == WStype_CONNECTED) {
    // Cliente conectado, aumentar o contador de usuários online
    usersOnline++;
    Serial.printf("Cliente [%u] conectado.\n", num);

    // Criar uma String temporária para a contagem de usuários
    String userCountMessage = "Users: " + String(usersOnline) + (usersOnline == 1 ? " user" : " users");
    webSocket.broadcastTXT(userCountMessage);
  }
  else if (type == WStype_DISCONNECTED) {
    // Cliente desconectado, diminuir o contador de usuários online
    usersOnline--;
    Serial.printf("Cliente [%u] desconectado.\n", num);

    // Criar uma String temporária para a contagem de usuários
    String userCountMessage = "Users: " + String(usersOnline) + (usersOnline == 1 ? " user" : " users");
    webSocket.broadcastTXT(userCountMessage);
  }
}

void setup() {
  Serial.begin(115200);

  // Configurar ESP8266 como ponto de acesso
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);

  // Servir página HTML
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", htmlPage);
    response->addHeader("Access-Control-Allow-Origin", "*"); // Para prevenir problemas de CORS
    response->addHeader("Content-Type", "text/html; charset=UTF-8"); // Definir o charset como UTF-8
    request->send(response);
  });

  // Iniciar servidor WebSocket
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  // Iniciar servidor web
  server.begin();

  Serial.println("Servidor iniciado");
}

void loop() {
  webSocket.loop();
}
