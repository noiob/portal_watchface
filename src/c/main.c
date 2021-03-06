#include <pebble.h>

Window *s_main_window;

static BitmapLayer *s_background_layer;
static GBitmap *s_background_bitmap;

static BitmapLayer *s_health_layer;
static GBitmap *s_health_bitmap;

static TextLayer *s_date_layer;
static GFont s_date_font;

static bool s_charging;
static bool s_disconnected;

#define TOTAL_IMAGE_SLOTS 4
#define NUMBER_OF_IMAGES 10
#define TOTAL_TILE_SLOTS 3
#define NUMBER_OF_TILES 20
#define EMPTY_SLOT -1
  
static int s_random_array[NUMBER_OF_TILES];

// bigger images for hour
const int IMAGE_RESOURCE_IDS_HOUR[NUMBER_OF_IMAGES] = {
  RESOURCE_ID_IMAGE_NUM_0, RESOURCE_ID_IMAGE_NUM_1, RESOURCE_ID_IMAGE_NUM_2,
  RESOURCE_ID_IMAGE_NUM_3, RESOURCE_ID_IMAGE_NUM_4, RESOURCE_ID_IMAGE_NUM_5,
  RESOURCE_ID_IMAGE_NUM_6, RESOURCE_ID_IMAGE_NUM_7, RESOURCE_ID_IMAGE_NUM_8,
  RESOURCE_ID_IMAGE_NUM_9
};

// smaller images for minutes
const int IMAGE_RESOURCE_IDS_MIN[NUMBER_OF_IMAGES] = {
  RESOURCE_ID_IMAGE_MIN_0, RESOURCE_ID_IMAGE_MIN_1, RESOURCE_ID_IMAGE_MIN_2, 
  RESOURCE_ID_IMAGE_MIN_3, RESOURCE_ID_IMAGE_MIN_4, RESOURCE_ID_IMAGE_MIN_5, 
  RESOURCE_ID_IMAGE_MIN_6, RESOURCE_ID_IMAGE_MIN_7, RESOURCE_ID_IMAGE_MIN_8,
  RESOURCE_ID_IMAGE_MIN_9
};

// tile images
const int IMAGE_RESOURCE_IDS_TILE[NUMBER_OF_TILES] = {
  RESOURCE_ID_IMAGE_TILE_0, RESOURCE_ID_IMAGE_TILE_1, RESOURCE_ID_IMAGE_TILE_2,
  RESOURCE_ID_IMAGE_TILE_3, RESOURCE_ID_IMAGE_TILE_4, RESOURCE_ID_IMAGE_TILE_5,
  RESOURCE_ID_IMAGE_TILE_6, RESOURCE_ID_IMAGE_TILE_7, RESOURCE_ID_IMAGE_TILE_8,
  RESOURCE_ID_IMAGE_TILE_9, RESOURCE_ID_IMAGE_TILE_10, RESOURCE_ID_IMAGE_TILE_11,
  RESOURCE_ID_IMAGE_TILE_12, RESOURCE_ID_IMAGE_TILE_13, RESOURCE_ID_IMAGE_TILE_14,
  RESOURCE_ID_IMAGE_TILE_15, RESOURCE_ID_IMAGE_TILE_16, RESOURCE_ID_IMAGE_TILE_17,
  RESOURCE_ID_IMAGE_TILE_18, RESOURCE_ID_IMAGE_TILE_19
};

static int s_image_slot_state[TOTAL_IMAGE_SLOTS] = {EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT};

static int s_tile_slot_state[TOTAL_TILE_SLOTS] = {EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT};

static GBitmap *s_images[TOTAL_IMAGE_SLOTS];
static BitmapLayer *s_image_layers[TOTAL_IMAGE_SLOTS];

static GBitmap *s_tiles[TOTAL_TILE_SLOTS];
static BitmapLayer *s_tile_layers[TOTAL_TILE_SLOTS];


static void load_status_image_into_slot(bool bluetooth);
static void unload_tile_image_from_slot(int slot_number);
static void load_tile_image_into_slot(int slot_number, int tile_number);

static void handle_health(HealthEventType event, void *context) {
  if (event == HealthEventMovementUpdate) {
    HealthMetric metric = HealthMetricStepCount;
    const HealthServiceTimeScope scope = HealthServiceTimeScopeDaily;
    
    time_t start = time_start_of_today();
    time_t end = start + SECONDS_PER_DAY;

    // Check the metric has data available for today
    HealthServiceAccessibilityMask mask1 = health_service_metric_accessible(metric, 
                                                                           start, end);
    // Check that an averaged value is accessible
    HealthServiceAccessibilityMask mask2 = 
      health_service_metric_averaged_accessible(metric, start, end, scope);
    
    if((mask1 & HealthServiceAccessibilityMaskAvailable) && (mask2 & HealthServiceAccessibilityMaskAvailable)) {
      int steps = (int)health_service_sum_today(metric);
      int average = (int)health_service_sum_averaged(metric, start, end, scope);
      printf("steps: %d, average: %d",steps,average);
      
      
      int percent = 100;
      if (steps <= average) {
        percent = steps*100/average;
      }
      layer_set_frame(bitmap_layer_get_layer(s_health_layer), GRect((percent * 1.2) + 22, 97, 119, 16));
    }
  }
}

static void bluetooth_connection_callback(bool connected) {
  if(connected){
    vibes_long_pulse();
    s_disconnected = false;
    unload_tile_image_from_slot(2);
    load_tile_image_into_slot(2, s_random_array[2]);
  }
  else{
    vibes_double_pulse();
    s_disconnected = true;
    load_status_image_into_slot(true);
  }
}

static void load_digit_image_into_slot(int slot_number, int digit_value) {

  if ((slot_number < 0) || (slot_number >= TOTAL_IMAGE_SLOTS)) {
    return;
  }

  if ((digit_value < 0) || (digit_value > 9)) {
    return;
  }

  if (s_image_slot_state[slot_number] != EMPTY_SLOT) {
    return;
  }
  
  s_image_slot_state[slot_number] = digit_value;
  
  // hour slot
  if(slot_number < 2){
    s_images[slot_number] = gbitmap_create_with_resource(IMAGE_RESOURCE_IDS_HOUR[digit_value]);
  }
  else{
    s_images[slot_number] = gbitmap_create_with_resource(IMAGE_RESOURCE_IDS_MIN[digit_value]);
  }
  
  GRect bounds = gbitmap_get_bounds(s_images[slot_number]);
  
  BitmapLayer *bitmap_layer;    
  if(slot_number < 2){
    bitmap_layer = bitmap_layer_create(GRect(((slot_number % 2) * 24) + 22, 9, bounds.size.w, bounds.size.h));
  }
  else{
    bitmap_layer = bitmap_layer_create(GRect(((slot_number % 2) * 12) + 78, 44, bounds.size.w, bounds.size.h));
  }
  s_image_layers[slot_number] = bitmap_layer;
  bitmap_layer_set_bitmap(bitmap_layer, s_images[slot_number]);
  Layer *window_layer = window_get_root_layer(s_main_window);
  layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_layer));
}

static void load_tile_image_into_slot(int slot_number, int tile_number) {

  if ((slot_number < 0) || (slot_number >= TOTAL_TILE_SLOTS)) {
    return;
  }

  if ((tile_number < 0) || (tile_number > (NUMBER_OF_TILES - 1))) {
    return;
  }

  if (s_tile_slot_state[slot_number] != EMPTY_SLOT) {
    return;
  }
  
  s_tile_slot_state[slot_number] = tile_number;
  
  s_tiles[slot_number] = gbitmap_create_with_resource(IMAGE_RESOURCE_IDS_TILE[tile_number]);
  
  GRect bounds = gbitmap_get_bounds(s_tiles[slot_number]);
  
  BitmapLayer *bitmap_layer;    
  
  bitmap_layer = bitmap_layer_create(GRect((slot_number * 41) + 24, 125, bounds.size.w, bounds.size.h));
  
  s_tile_layers[slot_number] = bitmap_layer;
  bitmap_layer_set_bitmap(bitmap_layer, s_tiles[slot_number]);
  Layer *window_layer = window_get_root_layer(s_main_window);
  layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_layer));
}

static void unload_digit_image_from_slot(int slot_number){
  if(s_image_slot_state[slot_number] != EMPTY_SLOT){
    layer_remove_from_parent(bitmap_layer_get_layer(s_image_layers[slot_number]));
    bitmap_layer_destroy(s_image_layers[slot_number]);
    gbitmap_destroy(s_images[slot_number]);
    s_image_slot_state[slot_number] = EMPTY_SLOT;
  }
}

static void unload_tile_image_from_slot(int slot_number){
  if(s_tile_slot_state[slot_number] != EMPTY_SLOT){
    layer_remove_from_parent(bitmap_layer_get_layer(s_tile_layers[slot_number]));
    bitmap_layer_destroy(s_tile_layers[slot_number]);
    gbitmap_destroy(s_tiles[slot_number]);
    s_tile_slot_state[slot_number] = EMPTY_SLOT;
  }
}

//true if bluetooth needs to be activated, else charging
static void load_status_image_into_slot(bool bluetooth){
  int slot_number;
  if(bluetooth){
    slot_number = 2;
    unload_tile_image_from_slot(slot_number);
    s_tiles[slot_number] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS_DISCONNECTED);
  }
  else{
    slot_number = 0;
    unload_tile_image_from_slot(slot_number);
    s_tiles[slot_number] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS_CHARGING);
  }
  
    
  s_tile_slot_state[slot_number] = bluetooth + 100;
  
  GRect bounds = gbitmap_get_bounds(s_tiles[slot_number]);
  
  BitmapLayer *bitmap_layer;    
  
  bitmap_layer = bitmap_layer_create(GRect((slot_number * 41) + 24, 125, bounds.size.w, bounds.size.h));
  
  s_tile_layers[slot_number] = bitmap_layer;
  bitmap_layer_set_bitmap(bitmap_layer, s_tiles[slot_number]);
  Layer *window_layer = window_get_root_layer(s_main_window);
  layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_layer));
}

static void display_value(unsigned short value, bool hour){
  //if hour == true then write into hour position, else minute
  value = value % 100;
  
  for(int position = 1; position >= 0; position--){
    int slot_number = (!hour * 2) + position;
    unload_digit_image_from_slot(slot_number);
    load_digit_image_into_slot(slot_number, value % 10);
    value = value / 10;
  }
}

static unsigned short get_display_hour(unsigned short hour) {
  if (clock_is_24h_style()) {
    return hour;
  }
  unsigned short display_hour = hour % 12;
  // Converts "0" to "12"
  return display_hour ? display_hour : 12;
}

static void update_tiles(){
  // switch around elements in list randomly, to avoid the same numbers
  for (int i = 0; i < TOTAL_TILE_SLOTS; i++){
    if(i == 0 && s_charging){
      continue;
    }
    else if(i == 2 && s_disconnected){
      continue;
    }
    int j = i + rand() % (NUMBER_OF_TILES - i);
    int temp = s_random_array[i];
    s_random_array[i] = s_random_array[j];
    s_random_array[j] = temp;
    unload_tile_image_from_slot(i);
    load_tile_image_into_slot(i, s_random_array[i]);
  } 
}

static void update_date(struct tm *tick_time){
  static char date_text[] = "01/01";
  strftime(date_text, sizeof(date_text), "%d/%m", tick_time);
  text_layer_set_text(s_date_layer, date_text);
}

static void update_time(struct tm *tick_time){
  display_value(get_display_hour(tick_time->tm_hour), true);
  display_value(tick_time->tm_min, false);
  if(!(tick_time->tm_min % 30)){
    update_tiles();
    if(!tick_time->tm_hour){
      update_date(tick_time);
    }
  }
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed){
  update_time(tick_time);
}

static void main_window_load(Window *window) {
  srand(time(NULL));
  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  s_background_layer = bitmap_layer_create(GRect(0, 0, 144, 168));
  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_background_layer));
  
  s_health_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_HIDE_HEALTH);
  s_health_layer = bitmap_layer_create(GRect(22, 97, 119, 16));
  bitmap_layer_set_bitmap(s_health_layer, s_health_bitmap);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_health_layer));
  
  s_date_layer = text_layer_create(GRect(28, 84, 30, 10));
  text_layer_set_text_color(s_date_layer, GColorBlack);
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text(s_date_layer, "01/01");
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_date_layer));
  s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_THOMA_9));
  text_layer_set_font(s_date_layer, s_date_font);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentLeft);
  
  for (int i = 0; i < NUMBER_OF_TILES; i++){
    s_random_array[i] = i;
  }
  
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);
  update_date(tick_time);
  update_time(tick_time);
  update_tiles();
  
  handle_health(HealthEventMovementUpdate, NULL);
  
  if(bluetooth_connection_service_peek()){
    s_disconnected = false;
    update_tiles();
  }
  else{
    s_disconnected = true;
    load_status_image_into_slot(true);
  }
  
  
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  bluetooth_connection_service_subscribe(bluetooth_connection_callback);
  health_service_events_subscribe(handle_health, NULL);
}

static void main_window_unload(Window *window){
  gbitmap_destroy(s_background_bitmap);
  bitmap_layer_destroy(s_background_layer);
  for(int i = 0; i < TOTAL_IMAGE_SLOTS; i++){
    unload_digit_image_from_slot(i);
  }
  for(int i = 0; i < TOTAL_TILE_SLOTS; i++){
    unload_tile_image_from_slot(i);
  }
  gbitmap_destroy(s_health_bitmap);
  bitmap_layer_destroy(s_health_layer);
  text_layer_destroy(s_date_layer);
  health_service_events_unsubscribe();
  bluetooth_connection_service_unsubscribe();
}

void handle_init(void) {
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
}

void handle_deinit(void) {
  window_destroy(s_main_window);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
