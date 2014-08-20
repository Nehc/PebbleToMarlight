
// Событие при запуске - можно протестировать доступность URL... Но я просто шлю сообщение
Pebble.addEventListener("ready",
							function(e) {
              Pebble.sendAppMessage({0: "Redy!"});
							});

// Обработка входящих сообщений: отправляем GET запрос, посылаем "эхо" обратно на часы
Pebble.addEventListener("appmessage",
                        function(e) {
                          var msg = e.payload.message; // SETTINGS : PEBBLE KIT JS : MESSAGE KEYS
                          var req = new XMLHttpRequest();
                          req.open('GET', "http://srv:8080/marlight.php?command="+msg, true);
                          req.send(null);
                          Pebble.sendAppMessage({0: msg});
                        });
