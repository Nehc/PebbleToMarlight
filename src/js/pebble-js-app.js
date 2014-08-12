// Простенькая функция на телефоне: отправляет http GET запрос по заданному URL 
Pebble.addEventListener("appmessage",
							function(e) {
              var req = new XMLHttpRequest();
              req.open('GET', "http://srv:8080/marlight.php?command="+e.payload.message, true); // e.payload.message - тот еще ребус 
              req.send(null);
							});