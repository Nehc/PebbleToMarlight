#include <pebble.h>

static Window *window;	
static AppTimer *timer;
static TextLayer *channel_layer;  
static TextLayer *command_layer;  

/* текущий канал управления */
static int channel;

/* структура для хранения состояния осей акселерометра*/
typedef struct Vector {
  int x;
  int y;
  int z;
} Vector;

/* введем три состояния осей: */
static Vector old; 	    //прошлое
static Vector current; 	//текущее
static Vector new; 	    //новое

/* Состояния осей акселерометра */
enum{
  HYRO_NN = 0, // нейтральное
  HYRO_UP = 1, // повернута вверх
  HYRO_DN = 2  // повернута вниз
};

/* Индекс для кортежа */  
enum {
	STATUS_KEY = 0, 	
	MESSAGE_KEY = 1
};

char channel_s[32], command_s[32]; 

#define ACCEL_STEP_MS 100 // частота опроса акселерометра (мс)
#define GRAVITY 900 // Ускорение свободного падения с точки зрения акселерометра 1000 +/- 100
  
/* Запись сообщения (параметр command) в буфер и отправка на телефон */
void send_message(char* command){
	DictionaryIterator *iter; // создание словаря
	app_message_outbox_begin(&iter); // начало сообщения
	dict_write_cstring(iter, MESSAGE_KEY, command); // отправка команды на телефон
  dict_write_end(iter); // конец сообщения
  app_message_outbox_send(); //отправка
}

static void timer_callback(void *data) {
  /* структура для хранения значений ускорения по всем трем осям */
  AccelData accel = (AccelData) { .x = 0, .y = 0, .z = 0 };

  /* для начала получим текущее значение ускорений */
  accel_service_peek(&accel);

  /* определим текущее положение часов по осям, 
  сравнивая показания акселерометра с гравитацией */
  if (accel.x > GRAVITY) new.x = HYRO_UP; 
  else if (accel.x < -GRAVITY) new.x = HYRO_DN;
  else new.x = HYRO_NN;
  if (accel.y > GRAVITY) new.y = HYRO_UP; 
  else if (accel.y < -GRAVITY) new.y = HYRO_DN;
  else new.y = HYRO_NN;
  if (accel.z > GRAVITY) new.z = HYRO_UP; 
  else if (accel.z < -GRAVITY) new.z = HYRO_DN;
  else new.z = HYRO_NN;
 
  
  if (current.x!= new.x) { /* если оно изменилось по оси х */
      /* зафиксируем изменение */
      old.x = current.x;
      current.x = new.x;
      /* проверим, как именно изменилось */
      if ((current.x == HYRO_DN)&&(old.x == HYRO_NN)){ // если руку подняли
        snprintf(command_s, sizeof("ON_0"), "ON_%d", channel); // то надо послать команду включения
        send_message(command_s);  /* посылаем сообщение */
        text_layer_set_text(command_layer, command_s);  /* показываем сообщение */
      } 
      if ((current.x == HYRO_UP)&&(old.x == HYRO_NN)){ // если руку опустили
        snprintf(command_s, sizeof("OFF_0"), "OFF_%d", channel); // то надо выключить
        send_message(command_s);  /* посылаем сообщение */
        text_layer_set_text(command_layer, command_s);  /* показываем сообщение */
      } 
    }
    if (current.y!= new.y) { /* если оно изменилось по оси y */
      /* зафиксируем изменение */
      old.y = current.y;
      current.y = new.y;
      /* проверим, как именно изменилось */
      if ((current.y == HYRO_UP)&&(old.y == HYRO_NN)){ // если повернули кисть влево 
        channel++; // то следующий по порядку канал 
        if (channel == 5) channel = 1; // по кругу
        snprintf(channel_s, sizeof("Текущий канал:0"), "Текущий канал:%d", channel);
        text_layer_set_text(channel_layer, channel_s);  /* показываем сообщение */
      } 
      if ((current.y == HYRO_DN)&&(old.y == HYRO_NN)){ // если повернули кисть вправо 
        channel--; // то предыдущий канал 
        if (channel == 0) channel = 4; // по кругу
        snprintf(channel_s, sizeof("Текущий канал:0"), "Текущий канал:%d", channel);
        text_layer_set_text(channel_layer, channel_s);  /* показываем сообщение */
      } 
    }
    if (current.z!= new.z) {  /* если оно изменилось по оси z */
      /* пока просто зафиксируем изменение */
       old.z = current.z;
      current.z = new.z;
    }
  /* презапустим ожидание таймера */
  timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}

TextLayer *config_text_layer(int16_t x,int16_t y,int16_t h,int16_t w, const char *font_key)  
{
  /* инициализация и настройка текстового слоя*/
  TextLayer *text_layer = text_layer_create(GRect(x, y, h, w)); // создаем слой, указываем размер и координаты 
  text_layer_set_text_color(text_layer, GColorWhite);  // цвет текста
  text_layer_set_background_color(text_layer, GColorClear);  // цвет фона
  text_layer_set_font(text_layer, fonts_get_system_font(font_key)); // шрифт
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter); // устанавливаем выравнивание по центру
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_layer));  // подключаем текстовый слой к основному окну
  return text_layer;
}

void init(void)
{
  /* Главное окно. */ 
  window = window_create(); // Создаем
  window_set_background_color(window, GColorBlack); // цвет 
  window_set_fullscreen(window, true); // полноэкранность 
  window_stack_push(window, true);  // открываем окно 

  /* Два текстовых слоя: для отображения... */
  channel_layer = config_text_layer(0, 3, 144, 80, FONT_KEY_GOTHIC_28);  // текущего канала
  command_layer = config_text_layer(0, 85, 144, 80, FONT_KEY_GOTHIC_28); // текущей команды 
    
  channel = 1; // по умолчанию у нас включен 1-ый канал
  snprintf(channel_s, sizeof("Текущий канал:0"), "Текущий канал:%d", channel);
  text_layer_set_text(channel_layer, channel_s);  /* показываем сообщение при запуске программы */
  
	/* Установка связи между телефоном и часами */
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
	
  /* Подписка на данные акселерометра */
  accel_data_service_subscribe(0, NULL);
  
  /* Подписка на таймер */
  timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);

}

void deinit(void) {
	app_message_deregister_callbacks();
	window_destroy(window);
}

int main( void ) {
	init();
	app_event_loop();
	deinit();
}