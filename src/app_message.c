#include <pebble.h>

static Window *window;	
static AppTimer *timer;
static TextLayer *channel_layer;  
static TextLayer *command_layer;  
static TextLayer *mode_layer;  

/* текущий канал управления и режим */
static int channel;
static bool Hyro;

/* структура для хранения состояния осей акселерометра*/
typedef struct Vector {
  int x, y, z;
} Vector;

/* определим две структуры для состояний: */
static Vector old; 	    //старое
static Vector new; 	    //новое

/* Состояния осей акселерометра */
enum{
  HYRO_NN = 0, // нейтральное
  HYRO_UP = 1, // повернута вверх
  HYRO_DN = 2  // повернута вниз
};

static const char* rooms[] = {"Детская", "Гостиная", "Спальня", "Ванная",}; // соответствие комнат каналам
static const char* mods[] = {"Управление жестами отключено", "Управление жестами активировано",}; // соответствие комнат каналам
                                 
static char command_s[32]; 
static bool OnOff[4];

#define ACCEL_STEP_MS 100 // частота опроса акселерометра (мс)
#define GRAVITY 900 // Ускорение свободного падения с точки зрения акселерометра pebble 1000 +/- 100
#define MESSAGE_KEY 0 //индекс для сообщений - отправляем всегда одно сообщение, поэтому достаточно одного 
  
/* Запись сообщения (параметр command) в буфер и отправка на телефон */
void send_message(char* command){
	DictionaryIterator *iter; // создание словаря
	app_message_outbox_begin(&iter); // начало сообщения
	dict_write_cstring(iter, MESSAGE_KEY, command); // отправка команды на телефон
  dict_write_end(iter); // конец сообщения
  app_message_outbox_send(); //отправка
}

// Обработка получения сообщения: просто выводим его в лог
static void in_received_handler(DictionaryIterator *received, void *context) {
	Tuple *tuple;

  tuple = dict_find(received, 0);
	if(tuple) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Received message: %s", tuple->value->cstring);
	}}

// Если что-то пошло не так при получении
static void in_dropped_handler(AppMessageResult reason, void *context) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Dropped!");  
}

// Если что-то пошло не так при отправке
static void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Fail!");  
  }



static void timer_callback(void *data) {
  /* структура для хранения значений ускорения по всем трем осям */
  AccelData accel = (AccelData) { .x = 0, .y = 0, .z = 0 };

  /* для начала получим текущее значение ускорений */
  accel_service_peek(&accel);

  //APP_LOG(APP_LOG_LEVEL_DEBUG, "x:%d, y:%d, z:%d", accel.x, accel.y, accel.z);
  
  /* определим текущее положение часов по осям, 
  сравнивая показания акселерометра с гравитацией */
  if (accel.x > GRAVITY) new.x = HYRO_DN;  
  else if (accel.x < -GRAVITY) new.x = HYRO_UP;
  else new.x = HYRO_NN;
  if (accel.y > GRAVITY) new.y = HYRO_DN; 
  else if (accel.y < -GRAVITY) new.y = HYRO_UP;
  else new.y = HYRO_NN;
  if (accel.z > GRAVITY) new.z = HYRO_DN; 
  else if (accel.z < -GRAVITY) new.z = HYRO_UP;
  else new.z = HYRO_NN;
 
  
  if (old.x!= new.x) { /* если оно изменилось по оси х */
    /* проверим, как именно изменилось */
    if (new.x == HYRO_UP){ // если руку подняли 
      snprintf(command_s, sizeof("ON_0"), "ON_%d", channel); // то надо послать команду включения
      send_message(command_s);  /* посылаем сообщение */
      text_layer_set_text(command_layer, "ВКЛ");  /* показываем сообщение */
      OnOff[channel-1] = 1;
    } 
    if (new.x == HYRO_DN){ // если руку опустили
      snprintf(command_s, sizeof("OFF_0"), "OFF_%d", channel); // то надо выключить
      send_message(command_s);  /* посылаем сообщение */
      text_layer_set_text(command_layer, "ВЫКЛ");  /* показываем сообщение */
      OnOff[channel-1] = 0;
    } 
    /* зафиксируем изменение */
    old.x = new.x;
  }
  
  if (old.y!= new.y) { /* если оно изменилось по оси y */
    /* проверим, как именно изменилось */
    if (new.y == HYRO_DN) channel++; // если повернули кисть влево то следующий по порядку канал 
    if (channel == 5) channel = 1; // по кругу
    if (new.y == HYRO_UP) channel--; // если в право то предыдущий канал 
    if (channel == 0) channel = 4; // по кругу
    
    text_layer_set_text(channel_layer, rooms[channel-1]); // показываем сообщение
    old.y = new.y; // зафиксируем изменение 
  }
  
  if (old.z!= new.z) {  /* если оно изменилось по оси z */
    /* пока просто зафиксируем изменение */
     old.z = new.z;
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

void clickMD(ClickRecognizerRef recognizer, void *context)  /* функция, срабатывающая при клике на центральную кнопку */
{
  OnOff[channel-1] = 1 - OnOff[channel-1]; //меняем значение выключателя на противоположный
  if(OnOff[channel-1] == 1) { //если надо включить свет
    snprintf(command_s, sizeof("ON_0"), "ON_%d", channel); // то надо послать команду включения
    send_message(command_s);  /* посылаем сообщение */
    text_layer_set_text(command_layer, "ВКЛ");  /* показываем сообщение */    
  } else { // иначе выключаем 
    snprintf(command_s, sizeof("OFF_0"), "OFF_%d", channel); // то надо выключить
    send_message(command_s);  /* посылаем сообщение */
    text_layer_set_text(command_layer, "ВЫКЛ");  /* показываем сообщение */
  } 
  
}

void clickUP(ClickRecognizerRef recognizer, void *context)  /* функция, срабатывающая при клике на верхнюю кнопку */
{
  channel++; // просто переключает каналы 
  if (channel == 5) channel = 1; // по кругу
  text_layer_set_text(channel_layer, rooms[channel-1]); // показываем сообщение
}

void clickDN(ClickRecognizerRef recognizer, void *context)  /* функция, срабатывающая при клике на нижнюю кнопку */
{
  Hyro = 1-Hyro; //меняем режим на противоположный 
  if (Hyro == 0) app_timer_cancel(timer); // если жесту отключены - отменяем подписку
  else timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL); // иначе - снова включаем
  text_layer_set_text(mode_layer, mods[Hyro]);  /* показываем сообщение */
}

void WindowsClickConfigProvider(void *context)  /* функция, внутри которой должны находиться подписки на кнопки */
{
    window_single_click_subscribe(BUTTON_ID_UP, clickUP); 
    window_single_click_subscribe(BUTTON_ID_SELECT, clickMD); 
    window_single_click_subscribe(BUTTON_ID_DOWN, clickDN); 
}

void init(void)
{
  /* Главное окно. */ 
  window = window_create(); // Создаем
  window_set_background_color(window, GColorBlack); // цвет 
  window_set_fullscreen(window, true); // полноэкранность 
  window_stack_push(window, true);  // открываем окно 

  /* Два текстовых слоя: для отображения... */
  channel_layer = config_text_layer(0, 3, 144, 40, FONT_KEY_GOTHIC_28);  // текущего канала
  command_layer = config_text_layer(0, 43, 144, 40, FONT_KEY_GOTHIC_28); // текущей команды 
  mode_layer = config_text_layer(0, 83, 144, 80, FONT_KEY_GOTHIC_24); // текущего режима   
  
  channel = 1; // по умолчанию у нас включен 1-ый канал
  Hyro = 1; // и управление жестами
  
  text_layer_set_text(channel_layer, rooms[channel-1]);  /* показываем сообщение при запуске программы */
  text_layer_set_text(mode_layer, mods[Hyro]);  /* показываем сообщение */
  
 	// Регистрация обработчиков событий AppMessage
	app_message_register_inbox_received(in_received_handler); 
	app_message_register_inbox_dropped(in_dropped_handler); 
	app_message_register_outbox_failed(out_failed_handler);

  /* определяем функцию, в которой будут находиться подписки на кнопки */
  window_set_click_config_provider(window, WindowsClickConfigProvider); 

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