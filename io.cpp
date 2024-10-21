#include <unistd.h>
#include <ncurses.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <vector>
#include <string>
#include <iostream>

#include "io.h"
#include "character.h"
#include "poke327.h"
#include "db_parse.h"

#define TRAINER_LIST_FIELD_WIDTH 46

int contains(const std::vector<int> &v, int target) {
  int n = v.size();
  for (int i = 0; i < n; i++) {
    if (v[i] == target) return 1;
  }
  return 0;
}

  int random_pokemon::setLevel() {
    int y_d = world.cur_idx[dim_y]-200;
    y_d = y_d < 0 ? y_d*-1 : y_d;
    int x_d = world.cur_idx[dim_x]-200;
    x_d = x_d < 0 ? x_d*-1 : x_d;
    int dist = y_d + x_d;
    
    int min = 0;
    int max = 0;
    
    int level = 0;
    if (dist < 2) { // min = 1; max = 1/2 = 0; can cause error
      level = 1;
    }
    else if (dist <= 200) {
      min = 1;
      max = dist/2;
      level = rand() % (max-min+1) + min;
    }
    else {
      min = (dist-200)/2;
      max = 100;
      level = rand() % (max-min+1) + min;
    }

    return level;
  }

  std::vector<int> random_pokemon::setMoves() {
    std::vector<int> move_list;

    int species_id = pokemon[randomPokemonIndex].species_id;
    for (auto &m : pokemon_moves) {
      if (m.pokemon_id == species_id && m.pokemon_move_method_id == 1 && m.level <= level && contains(move_list,m.move_id) == 0) {
        move_list.push_back(m.move_id);
      }
    }
    if (move_list.empty()) {
      move_list.push_back(165); // struggle
      return move_list;
    }
    else if (move_list.size() == 1) {
      return move_list;
    }
    else {
      int move_1_index = rand() % move_list.size();
      int move_1 = move_list.at(move_1_index);
      move_list.erase(move_list.begin() + move_1_index);
      int move_2 = move_list.at(rand() % move_list.size());
      return {move_1,move_2};
    }
  }

  std::vector<int> random_pokemon::getBaseStats() {
    std::vector<int> base;
    for (auto &bs : pokemon_stats) {
      if (bs.pokemon_id == pokemon[randomPokemonIndex].species_id) base.push_back(bs.base_stat);
      if (base.size() == 6) break;
    }
    return base;
  }

  std::vector<int> random_pokemon::genIVs() {
    std::vector<int> randIVs;
    for (int i = 0; i < 6; i++)
      randIVs.push_back(rand() % 16);

    return randIVs;
  }

  std::vector<int> random_pokemon::calculateStats() {
    std::vector<int> real_stats;
    
    int hp = (((base_stats[0] + IVs[0]) * 2) * level)/100 + level + 10;
    real_stats.push_back(hp);
    for (int i = 1; i < 6; i++) {
      int temp = (((base_stats[i] + IVs[i]) * 2) * level)/100 + 5;
      real_stats.push_back(temp);
    }


    return real_stats;
  }

typedef struct io_message {
  /* Will print " --more-- " at end of line when another message follows. *
   * Leave 10 extra spaces for that.                                      */
  char msg[71];
  struct io_message *next;
} io_message_t;

static io_message_t *io_head, *io_tail;

void io_init_terminal(void)
{
  initscr();
  raw();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  start_color();
  init_pair(COLOR_RED, COLOR_RED, COLOR_BLACK);
  init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
  init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
  init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
  init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
  init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
}

void io_reset_terminal(void)
{
  endwin();

  while (io_head) {
    io_tail = io_head;
    io_head = io_head->next;
    free(io_tail);
  }
  io_tail = NULL;
}

void io_queue_message(const char *format, ...)
{
  io_message_t *tmp;
  va_list ap;

  if (!(tmp = (io_message_t *) malloc(sizeof (*tmp)))) {
    perror("malloc");
    exit(1);
  }

  tmp->next = NULL;

  va_start(ap, format);

  vsnprintf(tmp->msg, sizeof (tmp->msg), format, ap);

  va_end(ap);

  if (!io_head) {
    io_head = io_tail = tmp;
  } else {
    io_tail->next = tmp;
    io_tail = tmp;
  }
}

static void io_print_message_queue(uint32_t y, uint32_t x)
{
  while (io_head) {
    io_tail = io_head;
    attron(COLOR_PAIR(COLOR_CYAN));
    mvprintw(y, x, "%-80s", io_head->msg);
    attroff(COLOR_PAIR(COLOR_CYAN));
    io_head = io_head->next;
    if (io_head) {
      attron(COLOR_PAIR(COLOR_CYAN));
      mvprintw(y, x + 70, "%10s", " --more-- ");
      attroff(COLOR_PAIR(COLOR_CYAN));
      refresh();
      getch();
    }
    free(io_tail);
  }
  io_tail = NULL;
}

/**************************************************************************
 * Compares trainer distances from the PC according to the rival distance *
 * map.  This gives the approximate distance that the PC must travel to   *
 * get to the trainer (doesn't account for crossing buildings).  This is  *
 * not the distance from the NPC to the PC unless the NPC is a rival.     *
 *                                                                        *
 * Not a bug.                                                             *
 **************************************************************************/
static int compare_trainer_distance(const void *v1, const void *v2)
{
  const character *const *c1 = (const character * const *) v1;
  const character *const *c2 = (const character * const *) v2;

  return (world.rival_dist[(*c1)->pos[dim_y]][(*c1)->pos[dim_x]] -
          world.rival_dist[(*c2)->pos[dim_y]][(*c2)->pos[dim_x]]);
}

static character *io_nearest_visible_trainer()
{
  character **c, *n;
  uint32_t x, y, count;

  c = (character **) malloc(world.cur_map->num_trainers * sizeof (*c));

  /* Get a linear list of trainers */
  for (count = 0, y = 1; y < MAP_Y - 1; y++) {
    for (x = 1; x < MAP_X - 1; x++) {
      if (world.cur_map->cmap[y][x] && world.cur_map->cmap[y][x] !=
          &world.pc) {
        c[count++] = world.cur_map->cmap[y][x];
      }
    }
  }

  /* Sort it by distance from PC */
  qsort(c, count, sizeof (*c), compare_trainer_distance);

  n = c[0];

  free(c);

  return n;
}

void io_display()
{
  uint32_t y, x;
  character *c;

  clear();
  for (y = 0; y < MAP_Y; y++) {
    for (x = 0; x < MAP_X; x++) {
      if (world.cur_map->cmap[y][x]) {
        mvaddch(y + 1, x, world.cur_map->cmap[y][x]->symbol);
      } else {
        switch (world.cur_map->map[y][x]) {
        case ter_boulder:
          attron(COLOR_PAIR(COLOR_MAGENTA));
          mvaddch(y + 1, x, BOULDER_SYMBOL);
          attroff(COLOR_PAIR(COLOR_MAGENTA));
          break;
        case ter_mountain:
          attron(COLOR_PAIR(COLOR_MAGENTA));
          mvaddch(y + 1, x, MOUNTAIN_SYMBOL);
          attroff(COLOR_PAIR(COLOR_MAGENTA));
          break;
        case ter_tree:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, TREE_SYMBOL);
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        case ter_forest:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, FOREST_SYMBOL);
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        case ter_path:
        case ter_bailey:
          attron(COLOR_PAIR(COLOR_YELLOW));
          mvaddch(y + 1, x, PATH_SYMBOL);
          attroff(COLOR_PAIR(COLOR_YELLOW));
          break;
        case ter_gate:
          attron(COLOR_PAIR(COLOR_YELLOW));
          mvaddch(y + 1, x, GATE_SYMBOL);
          attroff(COLOR_PAIR(COLOR_YELLOW));
          break;
        case ter_mart:
          attron(COLOR_PAIR(COLOR_BLUE));
          mvaddch(y + 1, x, POKEMART_SYMBOL);
          attroff(COLOR_PAIR(COLOR_BLUE));
          break;
        case ter_center:
          attron(COLOR_PAIR(COLOR_RED));
          mvaddch(y + 1, x, POKEMON_CENTER_SYMBOL);
          attroff(COLOR_PAIR(COLOR_RED));
          break;
        case ter_grass:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, TALL_GRASS_SYMBOL);
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        case ter_clearing:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, SHORT_GRASS_SYMBOL);
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        case ter_water:
          attron(COLOR_PAIR(COLOR_CYAN));
          mvaddch(y + 1, x, WATER_SYMBOL);
          attroff(COLOR_PAIR(COLOR_CYAN));
          break;
        default:
 /* Use zero as an error symbol, since it stands out somewhat, and it's *
  * not otherwise used.                                                 */
          attron(COLOR_PAIR(COLOR_CYAN));
          mvaddch(y + 1, x, ERROR_SYMBOL);
          attroff(COLOR_PAIR(COLOR_CYAN)); 
       }
      }
    }
  }

  mvprintw(23, 1, "PC position is (%2d,%2d) on map %d%cx%d%c.",
           world.pc.pos[dim_x],
           world.pc.pos[dim_y],
           abs(world.cur_idx[dim_x] - (WORLD_SIZE / 2)),
           world.cur_idx[dim_x] - (WORLD_SIZE / 2) >= 0 ? 'E' : 'W',
           abs(world.cur_idx[dim_y] - (WORLD_SIZE / 2)),
           world.cur_idx[dim_y] - (WORLD_SIZE / 2) <= 0 ? 'N' : 'S');
  mvprintw(22, 1, "%d known %s.", world.cur_map->num_trainers,
           world.cur_map->num_trainers > 1 ? "trainers" : "trainer");
  mvprintw(22, 30, "Nearest visible trainer: ");
  if ((c = io_nearest_visible_trainer())) {
    attron(COLOR_PAIR(COLOR_RED));
    mvprintw(22, 55, "%c at vector %d%cx%d%c.",
             c->symbol,
             abs(c->pos[dim_y] - world.pc.pos[dim_y]),
             ((c->pos[dim_y] - world.pc.pos[dim_y]) <= 0 ?
              'N' : 'S'),
             abs(c->pos[dim_x] - world.pc.pos[dim_x]),
             ((c->pos[dim_x] - world.pc.pos[dim_x]) <= 0 ?
              'W' : 'E'));
    attroff(COLOR_PAIR(COLOR_RED));
  } else {
    attron(COLOR_PAIR(COLOR_BLUE));
    mvprintw(22, 55, "NONE.");
    attroff(COLOR_PAIR(COLOR_BLUE));
  }

  io_print_message_queue(0, 0);

  refresh();
}

uint32_t io_teleport_pc(pair_t dest)
{
  /* Just for fun. And debugging.  Mostly debugging. */

  do {
    dest[dim_x] = rand_range(1, MAP_X - 2);
    dest[dim_y] = rand_range(1, MAP_Y - 2);
  } while (world.cur_map->cmap[dest[dim_y]][dest[dim_x]]                  ||
           move_cost[char_pc][world.cur_map->map[dest[dim_y]]
                                                [dest[dim_x]]] ==
             DIJKSTRA_PATH_MAX                                            ||
           world.rival_dist[dest[dim_y]][dest[dim_x]] < 0);

  return 0;
}

static void io_scroll_trainer_list(char (*s)[TRAINER_LIST_FIELD_WIDTH],
                                   uint32_t count)
{
  uint32_t offset;
  uint32_t i;

  offset = 0;

  while (1) {
    for (i = 0; i < 13; i++) {
      mvprintw(i + 6, 19, " %-40s ", s[i + offset]);
    }
    switch (getch()) {
    case KEY_UP:
      if (offset) {
        offset--;
      }
      break;
    case KEY_DOWN:
      if (offset < (count - 13)) {
        offset++;
      }
      break;
    case 27:
      return;
    }

  }
}

static void io_list_trainers_display(npc **c, uint32_t count)
{
  uint32_t i;
  char (*s)[TRAINER_LIST_FIELD_WIDTH]; /* pointer to array of 40 char */

  s = (char (*)[TRAINER_LIST_FIELD_WIDTH]) malloc(count * sizeof (*s));

  mvprintw(3, 19, " %-40s ", "");
  /* Borrow the first element of our array for this string: */
  snprintf(s[0], TRAINER_LIST_FIELD_WIDTH, "You know of %d trainers:", count);
  mvprintw(4, 19, " %-40s ", *s);
  mvprintw(5, 19, " %-40s ", "");

  for (i = 0; i < count; i++) {
    snprintf(s[i], TRAINER_LIST_FIELD_WIDTH, "%16s %c: %2d %s by %2d %s",
             char_type_name[c[i]->ctype],
             c[i]->symbol,
             abs(c[i]->pos[dim_y] - world.pc.pos[dim_y]),
             ((c[i]->pos[dim_y] - world.pc.pos[dim_y]) <= 0 ?
              "North" : "South"),
             abs(c[i]->pos[dim_x] - world.pc.pos[dim_x]),
             ((c[i]->pos[dim_x] - world.pc.pos[dim_x]) <= 0 ?
              "West" : "East"));
    if (count <= 13) {
      /* Handle the non-scrolling case right here. *
       * Scrolling in another function.            */
      mvprintw(i + 6, 19, " %-40s ", s[i]);
    }
  }

  if (count <= 13) {
    mvprintw(count + 6, 19, " %-40s ", "");
    mvprintw(count + 7, 19, " %-40s ", "Hit escape to continue.");
    while (getch() != 27 /* escape */)
      ;
  } else {
    mvprintw(19, 19, " %-40s ", "");
    mvprintw(20, 19, " %-40s ",
             "Arrows to scroll, escape to continue.");
    io_scroll_trainer_list(s, count);
  }

  free(s);
}

static void io_list_trainers()
{
  npc **c;
  uint32_t x, y, count;

  c = (npc **) malloc(world.cur_map->num_trainers * sizeof (*c));

  /* Get a linear list of trainers */
  for (count = 0, y = 1; y < MAP_Y - 1; y++) {
    for (x = 1; x < MAP_X - 1; x++) {
      if (world.cur_map->cmap[y][x] && world.cur_map->cmap[y][x] !=
          &world.pc) {
        c[count++] = dynamic_cast <npc *> (world.cur_map->cmap[y][x]);
      }
    }
  }

  /* Sort it by distance from PC */
  qsort(c, count, sizeof (*c), compare_trainer_distance);

  /* Display it */
  io_list_trainers_display(c, count);
  free(c);

  /* And redraw the map */
  io_display();
}

void io_pokemart()
{
  mvprintw(0, 0, "Welcome to the Pokemart. You have received 7 of each item.");
  refresh();
  world.pc.potions = 5;
  world.pc.pokeballs = 5;
  world.pc.revives = 5;
  getch();
}

void io_pokemon_center()
{
  mvprintw(0, 0, "Welcome to the Pokemon Center. Your pokemon are now healed.");
  refresh();
  for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
    world.pc.lineup[i].HP = world.pc.lineup[i].stats[0];
    world.pc.lineup[i].fainted = 0;
  }
  getch();
}

void send_out(character *c, int character_lineup_index) {
  std::string name = "Trainer " + c->name;
  if (c == &world.pc) name = "You";

  std::string pokemon_name = pokemon[c->lineup[character_lineup_index].randomPokemonIndex].identifier;
  if (c->lineup[character_lineup_index].is_shiny) pokemon_name += "(shiny!)";

  if (c != &world.pc) mvprintw(0,0, "%s sent out %s!",name.c_str(),pokemon_name.c_str());
  else mvprintw(1,0, "%s sent out %s!",name.c_str(),pokemon_name.c_str());
}

void send_out_mid(character *c, int character_lineup_index) {
  std::string name = "Trainer " + c->name;
  if (c == &world.pc) name = "You";

  std::string pokemon_name = pokemon[c->lineup[character_lineup_index].randomPokemonIndex].identifier;
  if (c->lineup[character_lineup_index].is_shiny) pokemon_name += "(shiny!)";
  
  move(15,0);
  clrtoeol();
  if (c != &world.pc) mvprintw(15,0, "%s sent out %s!",name.c_str(),pokemon_name.c_str());
  else mvprintw(15,0, "%s sent out %s!",name.c_str(),pokemon_name.c_str());
}

std::vector<std::string> get_pokemon_types(const random_pokemon &p) {
  std::vector<std::string> ret;
  for (auto &t : pokemon_types) {
    if (pokemon[p.randomPokemonIndex].id == t.pokemon_id) {
      ret.push_back(types[t.type_id]);
    }
  }
  return ret;
}

std::vector<int> get_pokemon_types_id(const random_pokemon &p) {
  std::vector<int> ret;
  for (auto &t : pokemon_types) {
    if (pokemon[p.randomPokemonIndex].id == t.pokemon_id) {
      ret.push_back(t.type_id);
    }
  }
  return ret;
}

int calculateDamage(random_pokemon *p, int move_choice) {
  float level = p->level;
  float power = moves[p->move_ids[move_choice-1]].power;
  float attack = p->stats[1];
  float defense = p->stats[2];
  float critical = rand() % 256 < p->base_stats[5]/2 ? 1.5 : 1;
  if (critical > 1.1) {
    move(15,0);
    clrtoeol();
    mvprintw(15,0,"Critical Hit!");
    getch();
  }
  float random = (rand() % (16) + 85) / 100.0;
  float STAB = 1;
  std::vector<int> p_types = get_pokemon_types_id(*p);
  for (int i = 0; i < (int)p_types.size(); i++) {
    if (p_types[i] == moves[p->move_ids[move_choice-1]].type_id) {
      STAB = 1.5;
    }
  }

  float step1=(((level*2/5+2) * power * attack/defense)/50 + 2);
  float damage = step1*critical*random*STAB;
  if (power == INT_MAX) return 1;
  return damage/1;
}

void pc_attack(random_pokemon *npc_pokemon, random_pokemon *pc_pokemon, int move_choice, std::string npc_pokemon_name_type_inf) {
  if (rand() % 100 <= moves[pc_pokemon->move_ids[move_choice-1]].accuracy) {
    std::string name = pokemon[pc_pokemon->randomPokemonIndex].identifier;
    std::string mov = moves[pc_pokemon->move_ids[move_choice-1]].identifier;
    
    move(15,0);
    clrtoeol();
    mvprintw(15,0, "Your %s used %s!",name.c_str(),mov.c_str());
    getch();
    move(15,0);
    clrtoeol();
    
    int damage = calculateDamage(pc_pokemon,move_choice);
    
    mvprintw(15,0, "It did %d damage to the %s!",damage,pokemon[npc_pokemon->randomPokemonIndex].identifier);
    npc_pokemon->HP = npc_pokemon->HP - damage < 0 ? 0 : npc_pokemon->HP-damage;
    move(2,0);
    clrtoeol();
    mvprintw(2,0,"%s | HP: %d/%d",npc_pokemon_name_type_inf.c_str(),npc_pokemon->HP,npc_pokemon->stats[0]);
    getch();
  }
  else {
    std::string name = pokemon[pc_pokemon->randomPokemonIndex].identifier;
    move(15,0);
    clrtoeol();
    mvprintw(15,0, "Your %s missed!",name.c_str());
    getch();
  }
}

void npc_attack(random_pokemon *npc_pokemon, random_pokemon *pc_pokemon, std::string pc_pokemon_name_type_inf) {
  int move_choice = rand() % (int)npc_pokemon->move_ids.size() + 1;
  if (rand() % 100 <= moves[npc_pokemon->move_ids[move_choice-1]].accuracy) {
    std::string name = pokemon[npc_pokemon->randomPokemonIndex].identifier;
    std::string mov = moves[npc_pokemon->move_ids[move_choice-1]].identifier;
    move(15,0);
    clrtoeol();
    mvprintw(15,0, "Enemy %s used %s!",name.c_str(),mov.c_str());
    getch();
    move(15,0);
    clrtoeol();
    
    int damage = calculateDamage(npc_pokemon,move_choice);
    
    mvprintw(15,0, "Enemy did %d damage to your %s!",damage,pokemon[pc_pokemon->randomPokemonIndex].identifier);
    pc_pokemon->HP = pc_pokemon->HP - damage < 0 ? 0 : pc_pokemon->HP-damage;
    move(7,0);
    clrtoeol();
    mvprintw(7,0,"%s | HP: %d/%d",pc_pokemon_name_type_inf.c_str(),pc_pokemon->HP,pc_pokemon->stats[0]);

    getch();
  }
  else {
    std::string name = pokemon[npc_pokemon->randomPokemonIndex].identifier;
    move(15,0);
    clrtoeol();
    mvprintw(15,0, "Enemy %s missed!",name.c_str());
    getch();
  }
}

int faintedPokemon(int action_number, std::vector<int> fainted_indices) {
  return fainted_indices[action_number];
}

int attack_seq(random_pokemon *npc_pokemon, random_pokemon *pc_pokemon, int move_choice,std::string pc_pokemon_name_type_inf,std::string npc_pokemon_name_type_inf, int wild = 0) {
  // 0 == no faints
  // 1 == trainer pokemon fainted
  // 2 == pc pokemon fainted
  // 3 == caught by pokeball
  // 4 == wild pokemon fled
  int npc_move = rand() % npc_pokemon->move_ids.size();
  int npc_priority = moves[npc_pokemon->move_ids[npc_move]].priority;
  int pc_priority = INT_MAX;
  if (move_choice <= (int)pc_pokemon->move_ids.size())
    pc_priority = moves[pc_pokemon->move_ids[move_choice-1]].priority;

  if (move_choice == -1) {
    npc_attack(npc_pokemon,pc_pokemon,pc_pokemon_name_type_inf);
    if (pc_pokemon->HP == 0) return 2;
    return 0;
  }

  else if (move_choice == 1 || move_choice == 2) {
    if (pc_priority > npc_priority) {
      // pc attacks then npc attacks if it doesnt faint
      pc_attack(npc_pokemon,pc_pokemon,move_choice,npc_pokemon_name_type_inf);
      if (npc_pokemon->HP == 0) return 1;
      npc_attack(npc_pokemon,pc_pokemon,pc_pokemon_name_type_inf);
      if (pc_pokemon->HP == 0) return 2;
    }
    else if (pc_priority == npc_priority) {
      if (pc_pokemon->stats[5] > npc_pokemon->stats[5]) {
        // pc attacks then npc attacks if it doesnt faint
        pc_attack(npc_pokemon,pc_pokemon,move_choice,npc_pokemon_name_type_inf);
        if (npc_pokemon->HP == 0) return 1;
        npc_attack(npc_pokemon,pc_pokemon,pc_pokemon_name_type_inf);
        if (pc_pokemon->HP == 0) return 2;
      }
      else if (pc_pokemon->stats[5] == npc_pokemon->stats[5]) {
        int temp = rand() % 2;
        if (!temp) {
          // pc attacks then npc attacks if it doesnt faint
          pc_attack(npc_pokemon,pc_pokemon,move_choice,npc_pokemon_name_type_inf);
          if (npc_pokemon->HP == 0) return 1;
          npc_attack(npc_pokemon,pc_pokemon,pc_pokemon_name_type_inf);
          if (pc_pokemon->HP == 0) return 2;
        }
        else {
          // npc then pc
          npc_attack(npc_pokemon,pc_pokemon,pc_pokemon_name_type_inf);
          if (pc_pokemon->HP == 0) return 2;
          pc_attack(npc_pokemon,pc_pokemon,move_choice,npc_pokemon_name_type_inf);
          if (npc_pokemon->HP == 0) return 1;
        }
      }
      else {
        // npc then pc
        npc_attack(npc_pokemon,pc_pokemon,pc_pokemon_name_type_inf);
        if (pc_pokemon->HP == 0) return 2;
        pc_attack(npc_pokemon,pc_pokemon,move_choice,npc_pokemon_name_type_inf);
        if (npc_pokemon->HP == 0) return 1;
      }
    }
    // npc attacks then pc attacks if it doesnt faint
    else {
      // npc then pc
      npc_attack(npc_pokemon,pc_pokemon,pc_pokemon_name_type_inf);
      if (pc_pokemon->HP == 0) return 2;
      pc_attack(npc_pokemon,pc_pokemon,move_choice,npc_pokemon_name_type_inf);
      if (npc_pokemon->HP == 0) return 1;
    }
  }
  else if (move_choice == 3) {
    world.pc.potions--;
    int old_hp = pc_pokemon->HP;
    pc_pokemon->HP = pc_pokemon->HP + 20 > pc_pokemon->stats[0] ? pc_pokemon->stats[0] : pc_pokemon->HP + 20;
    int healed = pc_pokemon->HP - old_hp;
    move(15,0);
    clrtoeol();
    mvprintw(15,0,"Used potion. Your %s healed %d hp.", pokemon[pc_pokemon->randomPokemonIndex].identifier,healed);
    move(7,0);
    clrtoeol();
    mvprintw(7,0,"%s | HP: %d/%d",pc_pokemon_name_type_inf.c_str(),pc_pokemon->HP,pc_pokemon->stats[0]);
    getch();
    npc_attack(npc_pokemon,pc_pokemon,pc_pokemon_name_type_inf);
  }
  else if (move_choice == 4) {
    int fainted_pokemon = 0;
    std::vector<int> fainted_indices = {0,0,0,0,0,0};
    for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
      if (world.pc.lineup[i].fainted) {
        fainted_pokemon++;
        fainted_indices[i] = 1;
      }
    }

    int y_i = 14;
    for (int i = 0; i < (int)fainted_indices.size(); i++) {
      if (fainted_indices[i]) {
        move(y_i,0);
        clrtoeol();
        mvprintw(y_i++,0,"%d: %s",i+1,pokemon[world.pc.lineup[i].randomPokemonIndex].identifier);
      }
    }

    int action_number = -2;
    echo();
    curs_set(1);
    
    do {
      if (action_number != -2 && (action_number < 1 || action_number > 6 || !faintedPokemon(action_number-1, fainted_indices))) {
        mvprintw(15,0,"Invalid Selection!");
      }
      move(13,0);
      clrtoeol();
      mvprintw(13,0,"Select a pokemon to revive (-1 to go back): ");
      int x = getcurx(stdscr);
      mvscanw(13, x, (char *) "%d", &action_number);
    } while (action_number != -1 && (action_number < 1 || action_number > 6 || !faintedPokemon(action_number-1, fainted_indices)));

    noecho();
    curs_set(0);

    y_i = 14;
    for (int i = 0; i < (int)fainted_indices.size(); i++) {
      if (fainted_indices[i]) {
        move(y_i,0);
        clrtoeol();
      }
    }

    if (action_number != -1) {
      move(15,0);
      clrtoeol();
      
      mvprintw(15,0,"Revived %s",pokemon[world.pc.lineup[action_number-1].randomPokemonIndex].identifier);
      world.pc.lineup[action_number-1].HP = world.pc.lineup[action_number-1].stats[0]/2;
      world.pc.lineup[action_number-1].fainted = 0;
      world.pc.revives--;
      
      getch();
    }
  }
  else if (move_choice == 5) { // pokeball
    move(15,0);
    clrtoeol();
    if (world.pc.pokeballs > 0) {
      world.pc.pokeballs--;
    }
    else {
      mvprintw(15,0,"Out of pokeballs...");
      getch();
      return 0;
    }
    if (world.pc.lineup.size() < 6) {
      world.pc.lineup.push_back(*npc_pokemon);
      mvprintw(15,0,"Caught %s!",pokemon[npc_pokemon->randomPokemonIndex].identifier);
      getch();
      return 3;
    }
    else {
      mvprintw(15,0,"The wild %s fled...",pokemon[npc_pokemon->randomPokemonIndex].identifier);
      getch();
      return 3;
    }
  }


  return 0;
}

void io_battle(character *aggressor, character *defender)
{
  int total_fainted = 0;
  for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
    if (world.pc.lineup[i].fainted) total_fainted++;
  }
  if (total_fainted == (int)world.pc.lineup.size()) return;
  npc *n = (npc *) ((aggressor == &world.pc) ? defender : aggressor);

  io_display();
  clear();
  mvprintw(0, 0, "Trainer %s approaches!", n->name.c_str());
  getch();

  int pc_pokemon_index = -1;
  for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
    if (!world.pc.lineup[i].fainted) {
      pc_pokemon_index = i;
      break;
    }
  }

  if (pc_pokemon_index == -1) {
    mvprintw(1,0,"Everyone has fainted...");
    getch();
    return;
  }

  send_out(n,0);
  send_out(&world.pc,0);
  getch();

  int npc_pokemon_index = 0;
  int battle_end = 0;
  while (!battle_end) {
    clear();

    
    // setup trainer
    attron(WA_UNDERLINE);
    mvprintw(0,0, "Trainer %s:", n->name.c_str());
    attroff(WA_UNDERLINE);
    int npc_alive = n->lineup.size();
    for (auto &it : n->lineup) {
      if (it.fainted) npc_alive--;
    }
    mvprintw(1,0, "# non-fainted pokemon: %d", npc_alive);
    std::string npc_pokemon_name = pokemon[n->lineup[npc_pokemon_index].randomPokemonIndex].identifier;
    if (n->lineup[npc_pokemon_index].is_shiny) npc_pokemon_name += "(shiny!)";
    std::vector<std::string> npc_pokemon_types = get_pokemon_types(n->lineup[npc_pokemon_index]);
    npc_pokemon_name += " (type(s): ";
    npc_pokemon_name += npc_pokemon_types[0];
    if (npc_pokemon_types.size() > 1) {
      npc_pokemon_name += ", ";
      npc_pokemon_name += npc_pokemon_types[1];
    }
    npc_pokemon_name += ")";
    
    std::string npc_move_names = moves[n->lineup[npc_pokemon_index].move_ids[0]].identifier;
    npc_move_names += " (type: ";
    npc_move_names += types[moves[n->lineup[npc_pokemon_index].move_ids[0]].type_id];
    npc_move_names += ")";

    if (n->lineup[npc_pokemon_index].move_ids.size() > 1) {
      npc_move_names += " | 2: ";
      npc_move_names += moves[n->lineup[npc_pokemon_index].move_ids[1]].identifier;
      npc_move_names += " (type: ";
      npc_move_names += types[moves[n->lineup[npc_pokemon_index].move_ids[1]].type_id];
      npc_move_names += ")";
    }

    mvprintw(2,0,"%s | HP: %d/%d",npc_pokemon_name.c_str(),n->lineup[npc_pokemon_index].HP,n->lineup[npc_pokemon_index].stats[0]);
    mvprintw(3,0,"Moves: 1: %s",npc_move_names.c_str());



    // setup pc
    attron(WA_UNDERLINE);
    mvprintw(5,0,"You:");
    attroff(WA_UNDERLINE);
    std::string pc_pokemon_name = pokemon[world.pc.lineup[pc_pokemon_index].randomPokemonIndex].identifier;
    if (world.pc.lineup[pc_pokemon_index].is_shiny) pc_pokemon_name += "(shiny!)";
    std::vector<std::string> pc_pokemon_types = get_pokemon_types(world.pc.lineup[pc_pokemon_index]);
    pc_pokemon_name += " (type(s): ";
    pc_pokemon_name += pc_pokemon_types[0];
    if (pc_pokemon_types.size() > 1) {
      pc_pokemon_name += ", ";
      pc_pokemon_name += pc_pokemon_types[1];
    }
    pc_pokemon_name += ")";
    
    std::string pc_move_names = moves[world.pc.lineup[pc_pokemon_index].move_ids[0]].identifier;
    pc_move_names += " (type: ";
    pc_move_names += types[moves[world.pc.lineup[pc_pokemon_index].move_ids[0]].type_id];
    pc_move_names += ")";

    if (world.pc.lineup[pc_pokemon_index].move_ids.size() > 1) {
      pc_move_names += " | 2: ";
      pc_move_names += moves[world.pc.lineup[pc_pokemon_index].move_ids[1]].identifier;
      pc_move_names += " (type: ";
      pc_move_names += types[moves[world.pc.lineup[pc_pokemon_index].move_ids[1]].type_id];
      pc_move_names += ")";
    }


    
    mvprintw(7,0,"%s | HP: %d/%d",pc_pokemon_name.c_str(),world.pc.lineup[pc_pokemon_index].HP,world.pc.lineup[pc_pokemon_index].stats[0]);
    mvprintw(8,0,"Moves:");
    mvprintw(9,0,"1: %s",pc_move_names.c_str());
    mvprintw(10,0,"Bag:");
    mvprintw(11,0,"3: Use Potion | 4: Use Revive | 7: Swap");



    // handle choices
    int action_number = -1;
    echo();
    curs_set(1);
    
    do {
      if (action_number == 3 && world.pc.potions == 0) mvprintw(15,0,"Out of Potions!");
      else if (action_number == 4 && world.pc.revives == 0) mvprintw(15,0, "Out of Revives!");
      move(13,0);
      clrtoeol();
      mvprintw(13,0,"Select an action: ");
      int x = getcurx(stdscr);
      mvscanw(13, x, (char *) "%d", &action_number);
      if ((action_number < 1 || action_number > 4 || (action_number == 2 && world.pc.lineup[pc_pokemon_index].move_ids.size() == 1)) && action_number != 7) mvprintw(15,0,"Invalid Input!");
    } while (action_number != 7 && (action_number < 1 || action_number > 4 || (action_number == 3 && world.pc.potions == 0) || (action_number == 4 && world.pc.revives == 0) ||  (action_number == 2 && world.pc.lineup[pc_pokemon_index].move_ids.size() == 1)));

    noecho();
    curs_set(0);

    int flee= 0;
    if (action_number == 7) {
      int fainted_total = 0;
      std::vector<int> fainted_indices;
      for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
        if (world.pc.lineup[i].fainted) {
          fainted_indices.push_back(1);
          fainted_total++;
        }
        else fainted_indices.push_back(0);
      }

      if (fainted_total == (int)world.pc.lineup.size()) {
        move(15,0);
        clrtoeol();
        mvprintw(15,0,"Everyone fainted...");
        getch();
        break;
      } 

      int y_i = 14;
      for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
        if (!fainted_indices[i]) {
          move(y_i,0);
          clrtoeol();
          mvprintw(y_i++,0,"%d: %s", i+1, pokemon[world.pc.lineup[i].randomPokemonIndex].identifier);
        }
      }

      int choice = -1;
      echo();
      curs_set(1);
      do {
        
        move(13,0);
        clrtoeol();
        mvprintw(13,0, "Choose a pokemon to swap in: ");
        int x = getcurx(stdscr);
        mvscanw(13, x, (char *) "%d", &choice);
      } while (choice < 1 || choice > (int)world.pc.lineup.size() || fainted_indices[choice-1]);
      noecho();
      curs_set(0);
      flee = 1;
      pc_pokemon_index = choice-1;
      
      pc_pokemon_name = pokemon[world.pc.lineup[pc_pokemon_index].randomPokemonIndex].identifier;
    if (world.pc.lineup[pc_pokemon_index].is_shiny) pc_pokemon_name += "(shiny!)";
    pc_pokemon_types = get_pokemon_types(world.pc.lineup[pc_pokemon_index]);
    pc_pokemon_name += " (type(s): ";
    pc_pokemon_name += pc_pokemon_types[0];
    if (pc_pokemon_types.size() > 1) {
      pc_pokemon_name += ", ";
      pc_pokemon_name += pc_pokemon_types[1];
    }
    pc_pokemon_name += ")";
    
    pc_move_names = moves[world.pc.lineup[pc_pokemon_index].move_ids[0]].identifier;
    pc_move_names += " (type: ";
    pc_move_names += types[moves[world.pc.lineup[pc_pokemon_index].move_ids[0]].type_id];
    pc_move_names += ")";

    if (world.pc.lineup[pc_pokemon_index].move_ids.size() > 1) {
      pc_move_names += " | 2: ";
      pc_move_names += moves[world.pc.lineup[pc_pokemon_index].move_ids[1]].identifier;
      pc_move_names += " (type: ";
      pc_move_names += types[moves[world.pc.lineup[pc_pokemon_index].move_ids[1]].type_id];
      pc_move_names += ")";
    }
    move(7,0);
    clrtoeol();
    move(9,0);
    clrtoeol();
    mvprintw(7,0,"%s | HP: %d/%d",pc_pokemon_name.c_str(),world.pc.lineup[pc_pokemon_index].HP,world.pc.lineup[pc_pokemon_index].stats[0]);
    mvprintw(8,0,"Moves:");
    mvprintw(9,0,"1: %s",pc_move_names.c_str());
    y_i = 14;
      for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
        move(y_i++,0);
        clrtoeol();
      }
    }
    int fainted;
    if (flee) fainted = attack_seq(&n->lineup[npc_pokemon_index],&world.pc.lineup[pc_pokemon_index],-1,pc_pokemon_name,npc_pokemon_name);
    else fainted = attack_seq(&n->lineup[npc_pokemon_index],&world.pc.lineup[pc_pokemon_index],action_number,pc_pokemon_name,npc_pokemon_name);
    
    move(15,0);
    clrtoeol();
    if (fainted == 0) {
      // nothing special
      continue;
    }
    else if (fainted == 1) {
      mvprintw(15,0,"Enemy %s fainted!",pokemon[n->lineup[npc_pokemon_index].randomPokemonIndex].identifier);
      getch();
      n->lineup[npc_pokemon_index].fainted = 1;
      if (++npc_pokemon_index >= (int)n->lineup.size()) {
        battle_end = 1;
        break;
      }
      else {
        send_out_mid(n,npc_pokemon_index);
        getch();
      }
    }
    else if (fainted == 2) {
      mvprintw(15,0,"Your %s fainted!",pokemon[world.pc.lineup[pc_pokemon_index].randomPokemonIndex].identifier);
      world.pc.lineup[pc_pokemon_index].fainted = 1;
      // TODO implement losing
      getch();
      int fainted_total = 0;
      std::vector<int> fainted_indices;
      for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
        if (world.pc.lineup[i].fainted) {
          fainted_indices.push_back(1);
          fainted_total++;
        }
        else fainted_indices.push_back(0);
      }

      if (fainted_total == (int)world.pc.lineup.size()) {
        move(15,0);
        clrtoeol();
        mvprintw(15,0,"Everyone fainted...");
        getch();
        break;
      } 

      int y_i = 14;
      for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
        if (!fainted_indices[i]) {
          move(y_i,0);
          clrtoeol();
          mvprintw(y_i++,0,"%d: %s", i+1, pokemon[world.pc.lineup[i].randomPokemonIndex].identifier);
        }
      }

      int choice = -1;
      echo();
      curs_set(1);
      do {
        
        move(13,0);
        clrtoeol();
        mvprintw(13,0, "Choose a pokemon to swap in: ");
        int x = getcurx(stdscr);
        mvscanw(13, x, (char *) "%d", &choice);
      } while (choice < 1 || choice > (int)world.pc.lineup.size() || fainted_indices[choice-1]);
      noecho();
      curs_set(0);

      pc_pokemon_index = choice-1;

      y_i = 14;
      for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
        move(y_i++,0);
        clrtoeol();
      }
    }

  }
  // battle_end == 1 trainer won
  // battle_end == 2 npc won
  // battle_end == 3 pc fled
  // battle_end == 4 wild pokemon fled
  // battle_end == 5 caught
  
 
  n->defeated = 1;
  if (n->ctype == char_hiker || n->ctype == char_rival) {
    n->mtype = move_wander;
  }
}

void send_out_enc(character *c, int character_lineup_index) {
  std::string name = "Trainer " + c->name;
  if (c == &world.pc) name = "You";

  std::string pokemon_name = pokemon[c->lineup[character_lineup_index].randomPokemonIndex].identifier;
  if (c->lineup[character_lineup_index].is_shiny) pokemon_name += "(shiny!)";

  move(1,0);
  clrtoeol();
  mvprintw(1,0, "%s sent out %s!",name.c_str(),pokemon_name.c_str());
}

void io_encounter() {

  random_pokemon p = random_pokemon();
  std::string movez = "";
  for (int i = 0; i < (int)p.move_ids.size(); i++) {
    movez = movez + moves[p.move_ids[i]].identifier;
    if (p.move_ids.size() == 2 && i == 0)
      movez = movez + ",";
  }

  std::string name = pokemon[p.randomPokemonIndex].identifier;
  if (p.is_shiny) name += "(shiny!)";

  io_display();
  clear();
  mvprintw(0, 0, "A wild %s appeared!", name.c_str());
  getch();

  int pc_pokemon_index = -1;
  for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
    if (!world.pc.lineup[i].fainted) {
      pc_pokemon_index = i;
      break;
    }
  }

  if (pc_pokemon_index == -1) {
    mvprintw(1,0,"Everyone has fainted...");
    getch();
    return;
  }

  send_out_enc(&world.pc,pc_pokemon_index);
  getch();


  //int npc_pokemon_index = 0;
  int battle_end = 0;
  while (!battle_end) {
    clear();

    
    // setup trainer
    attron(WA_UNDERLINE);
    mvprintw(0,0, "%s:", name.c_str());
    attroff(WA_UNDERLINE);
    std::string npc_pokemon_name = name;
    std::vector<std::string> npc_pokemon_types = get_pokemon_types(p);
    npc_pokemon_name += " (type(s): ";
    npc_pokemon_name += npc_pokemon_types[0];
    if (npc_pokemon_types.size() > 1) {
      npc_pokemon_name += ", ";
      npc_pokemon_name += npc_pokemon_types[1];
    }
    npc_pokemon_name += ")";
    
    std::string npc_move_names = moves[p.move_ids[0]].identifier;
    npc_move_names += " (type: ";
    npc_move_names += types[moves[p.move_ids[0]].type_id];
    npc_move_names += ")";

    if (p.move_ids.size() > 1) {
      npc_move_names += " | 2: ";
      npc_move_names += moves[p.move_ids[1]].identifier;
      npc_move_names += " (type: ";
      npc_move_names += types[moves[p.move_ids[1]].type_id];
      npc_move_names += ")";
    }

    mvprintw(2,0,"%s | HP: %d/%d",npc_pokemon_name.c_str(),p.HP,p.stats[0]);
    mvprintw(3,0,"Moves: 1: %s",npc_move_names.c_str());



    // setup pc
    attron(WA_UNDERLINE);
    mvprintw(5,0,"You:");
    attroff(WA_UNDERLINE);
    std::string pc_pokemon_name = pokemon[world.pc.lineup[pc_pokemon_index].randomPokemonIndex].identifier;
    if (world.pc.lineup[pc_pokemon_index].is_shiny) pc_pokemon_name += "(shiny!)";
    std::vector<std::string> pc_pokemon_types = get_pokemon_types(world.pc.lineup[pc_pokemon_index]);
    pc_pokemon_name += " (type(s): ";
    pc_pokemon_name += pc_pokemon_types[0];
    if (pc_pokemon_types.size() > 1) {
      pc_pokemon_name += ", ";
      pc_pokemon_name += pc_pokemon_types[1];
    }
    pc_pokemon_name += ")";
    
    std::string pc_move_names = moves[world.pc.lineup[pc_pokemon_index].move_ids[0]].identifier;
    pc_move_names += " (type: ";
    pc_move_names += types[moves[world.pc.lineup[pc_pokemon_index].move_ids[0]].type_id];
    pc_move_names += ")";

    if (world.pc.lineup[pc_pokemon_index].move_ids.size() > 1) {
      pc_move_names += " | 2: ";
      pc_move_names += moves[world.pc.lineup[pc_pokemon_index].move_ids[1]].identifier;
      pc_move_names += " (type: ";
      pc_move_names += types[moves[world.pc.lineup[pc_pokemon_index].move_ids[1]].type_id];
      pc_move_names += ")";
    }


    
    mvprintw(7,0,"%s | HP: %d/%d",pc_pokemon_name.c_str(),world.pc.lineup[pc_pokemon_index].HP,world.pc.lineup[pc_pokemon_index].stats[0]);
    mvprintw(8,0,"Moves:");
    mvprintw(9,0,"1: %s",pc_move_names.c_str());
    mvprintw(10,0,"Bag:");
    mvprintw(11,0,"3: Use Potion | 4: Use Revive | 5: Throw Pokeball | 6: Flee | 7: Swap");



    // handle choices
    int action_number = -1;
    echo();
    curs_set(1);
    
    do {
      if (action_number == 3 && world.pc.potions == 0) mvprintw(15,0,"Out of Potions!");
      else if (action_number == 4 && world.pc.revives == 0) mvprintw(15,0, "Out of Revives!");
      else if (action_number == 5 && world.pc.pokeballs == 0) mvprintw(15,0, "Out of Pokeballs!");
      move(13,0);
      clrtoeol();
      mvprintw(13,0,"Select an action: ");
      int x = getcurx(stdscr);
      mvscanw(13, x, (char *) "%d", &action_number);
      if (action_number < 1 || action_number > 7 || (action_number == 2 && world.pc.lineup[pc_pokemon_index].move_ids.size() == 1)) mvprintw(15,0,"Invalid Input!");
    } while (action_number < 1 || action_number > 7 || (action_number == 3 && world.pc.potions == 0) || (action_number == 5 && world.pc.pokeballs == 0) || (action_number == 4 && world.pc.revives == 0) ||  (action_number == 2 && world.pc.lineup[pc_pokemon_index].move_ids.size() == 1));

    noecho();
    curs_set(0);

    int flee = 0;
    if (action_number == 6) {
      if (rand() % 2 == 1) {
        move(15,0);
        clrtoeol();
        mvprintw(15,0,"Couldn't Flee!");
        getch();
        flee = 1;
      }
      else {
        move(15,0);
        clrtoeol();
        mvprintw(15,0,"Successfully fled...");
        getch();
        battle_end = 3;
        break;
      }
    } else if (action_number == 7) {
      int fainted_total = 0;
      std::vector<int> fainted_indices;
      for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
        if (world.pc.lineup[i].fainted) {
          fainted_indices.push_back(1);
          fainted_total++;
        }
        else fainted_indices.push_back(0);
      }

      if (fainted_total == (int)world.pc.lineup.size()) {
        move(15,0);
        clrtoeol();
        mvprintw(15,0,"Everyone fainted...");
        getch();
        break;
      } 

      int y_i = 14;
      for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
        if (!fainted_indices[i]) {
          move(y_i,0);
          clrtoeol();
          mvprintw(y_i++,0,"%d: %s", i+1, pokemon[world.pc.lineup[i].randomPokemonIndex].identifier);
        }
      }

      int choice = -1;
      echo();
      curs_set(1);
      do {
        
        move(13,0);
        clrtoeol();
        mvprintw(13,0, "Choose a pokemon to swap in: ");
        int x = getcurx(stdscr);
        mvscanw(13, x, (char *) "%d", &choice);
      } while (choice < 1 || choice > (int)world.pc.lineup.size() || fainted_indices[choice-1]);
      noecho();
      curs_set(0);
      flee = 1;
      pc_pokemon_index = choice-1;
      
      pc_pokemon_name = pokemon[world.pc.lineup[pc_pokemon_index].randomPokemonIndex].identifier;
    if (world.pc.lineup[pc_pokemon_index].is_shiny) pc_pokemon_name += "(shiny!)";
    pc_pokemon_types = get_pokemon_types(world.pc.lineup[pc_pokemon_index]);
    pc_pokemon_name += " (type(s): ";
    pc_pokemon_name += pc_pokemon_types[0];
    if (pc_pokemon_types.size() > 1) {
      pc_pokemon_name += ", ";
      pc_pokemon_name += pc_pokemon_types[1];
    }
    pc_pokemon_name += ")";
    
    pc_move_names = moves[world.pc.lineup[pc_pokemon_index].move_ids[0]].identifier;
    pc_move_names += " (type: ";
    pc_move_names += types[moves[world.pc.lineup[pc_pokemon_index].move_ids[0]].type_id];
    pc_move_names += ")";

    if (world.pc.lineup[pc_pokemon_index].move_ids.size() > 1) {
      pc_move_names += " | 2: ";
      pc_move_names += moves[world.pc.lineup[pc_pokemon_index].move_ids[1]].identifier;
      pc_move_names += " (type: ";
      pc_move_names += types[moves[world.pc.lineup[pc_pokemon_index].move_ids[1]].type_id];
      pc_move_names += ")";
    }
    move(7,0);
    clrtoeol();
    move(9,0);
    clrtoeol();
    mvprintw(7,0,"%s | HP: %d/%d",pc_pokemon_name.c_str(),world.pc.lineup[pc_pokemon_index].HP,world.pc.lineup[pc_pokemon_index].stats[0]);
    mvprintw(8,0,"Moves:");
    mvprintw(9,0,"1: %s",pc_move_names.c_str());
    y_i = 14;
      for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
        move(y_i++,0);
        clrtoeol();
      }
    }
    int fainted;
    if (flee) fainted = attack_seq(&p,&world.pc.lineup[pc_pokemon_index],-1,pc_pokemon_name,npc_pokemon_name);
    else fainted = attack_seq(&p,&world.pc.lineup[pc_pokemon_index],action_number,pc_pokemon_name,npc_pokemon_name);
    
    move(15,0);
    clrtoeol();
    if (fainted == 0) {
      // nothing special
      continue;
    }
    else if (fainted == 1) {
      mvprintw(15,0,"Enemy %s fainted!",name.c_str());
      getch();
      battle_end = 1;
    }
    else if (fainted == 2) {
      mvprintw(15,0,"Your %s fainted!",pokemon[world.pc.lineup[pc_pokemon_index].randomPokemonIndex].identifier);
      world.pc.lineup[pc_pokemon_index].fainted = 1;
      // TODO implement losing
      getch();
      int fainted_total = 0;
      std::vector<int> fainted_indices;
      for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
        if (world.pc.lineup[i].fainted) {
          fainted_indices.push_back(1);
          fainted_total++;
        }
        else fainted_indices.push_back(0);
      }

      if (fainted_total == (int)world.pc.lineup.size()) {
        move(15,0);
        clrtoeol();
        mvprintw(15,0,"Everyone fainted...");
        getch();
        break;
      } 

      int y_i = 14;
      for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
        if (!fainted_indices[i]) {
          move(y_i,0);
          clrtoeol();
          mvprintw(y_i++,0,"%d: %s", i+1, pokemon[world.pc.lineup[i].randomPokemonIndex].identifier);
        }
      }

      int choice = -1;
      echo();
      curs_set(1);
      do {
        
        move(13,0);
        clrtoeol();
        mvprintw(13,0, "Choose a pokemon to swap in: ");
        int x = getcurx(stdscr);
        mvscanw(13, x, (char *) "%d", &choice);
      } while (choice < 1 || choice > (int)world.pc.lineup.size() || fainted_indices[choice-1]);
      noecho();
      curs_set(0);

      pc_pokemon_index = choice-1;

      y_i = 14;
      for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
        move(y_i++,0);
        clrtoeol();
      }

    }
    else if (fainted == 3) {
      battle_end = 5;
      break;
    }

    // battle_end == 1 trainer won
  // battle_end == 2 npc won
  // battle_end == 3 pc fled
  // battle_end == 4 wild pokemon fled
  // battle_end == 5 caught

  }


}

void io_starter() {
  std::vector<random_pokemon> starter;
  random_pokemon p1 = random_pokemon();
  random_pokemon p2 = random_pokemon();
  random_pokemon p3 = random_pokemon();
  starter.push_back(p1);
  starter.push_back(p2);
  starter.push_back(p3);

  std::string name1 = pokemon[p1.randomPokemonIndex].identifier;
  if (p1.is_shiny) name1 += "(shiny!)";
  std::string name2 = pokemon[p2.randomPokemonIndex].identifier;
  if (p2.is_shiny) name2 += "(shiny!)";
  std::string name3 = pokemon[p3.randomPokemonIndex].identifier;
  if (p3.is_shiny) name3 += "(shiny!)";
  
  int choice = -1;
  echo();
  curs_set(1);
  do {
    mvprintw(0,0, "0:%s|1:%s|2:%s|Choice: ",name1.c_str(),name2.c_str(),name3.c_str());
    int x = getcurx(stdscr);
    mvscanw(0, x, (char *) "%d", &choice);
    refresh();
  } while (choice < 0 || choice > 2);
  
  refresh();
  noecho();
  curs_set(0);

  world.pc.lineup.push_back(starter[choice]);

  io_display();
  mvprintw(0,0,"Chose %s", choice == 0 ? name1.c_str() : choice == 1 ? name2.c_str() : name3.c_str());
  refresh();
  getch();

}

uint32_t move_pc_dir(uint32_t input, pair_t dest)
{
  dest[dim_y] = world.pc.pos[dim_y];
  dest[dim_x] = world.pc.pos[dim_x];

  switch (input) {
  case 1:
  case 2:
  case 3:
    dest[dim_y]++;
    break;
  case 4:
  case 5:
  case 6:
    break;
  case 7:
  case 8:
  case 9:
    dest[dim_y]--;
    break;
  }
  switch (input) {
  case 1:
  case 4:
  case 7:
    dest[dim_x]--;
    break;
  case 2:
  case 5:
  case 8:
    break;
  case 3:
  case 6:
  case 9:
    dest[dim_x]++;
    break;
  case '>':
    if (world.cur_map->map[world.pc.pos[dim_y]][world.pc.pos[dim_x]] ==
        ter_mart) {
      io_pokemart();
    }
    if (world.cur_map->map[world.pc.pos[dim_y]][world.pc.pos[dim_x]] ==
        ter_center) {
      io_pokemon_center();
    }
    break;
  }

  if (world.cur_map->cmap[dest[dim_y]][dest[dim_x]]) {
    if (dynamic_cast<npc *> (world.cur_map->cmap[dest[dim_y]][dest[dim_x]]) &&
        ((npc *) world.cur_map->cmap[dest[dim_y]][dest[dim_x]])->defeated) {
      // Some kind of greeting here would be nice
      return 1;
    } else if ((dynamic_cast<npc *>
                (world.cur_map->cmap[dest[dim_y]][dest[dim_x]]))) {
      io_battle(&world.pc, world.cur_map->cmap[dest[dim_y]][dest[dim_x]]);
      // Not actually moving, so set dest back to PC position
      dest[dim_x] = world.pc.pos[dim_x];
      dest[dim_y] = world.pc.pos[dim_y];
    }
  }
  
  if (move_cost[char_pc][world.cur_map->map[dest[dim_y]][dest[dim_x]]] ==
      DIJKSTRA_PATH_MAX) {
    return 1;
  }

  return 0;
}

void io_teleport_world(pair_t dest)
{
  /* mvscanw documentation is unclear about return values.  I believe *
   * that the return value works the same way as scanf, but instead   *
   * of counting on that, we'll initialize x and y to out of bounds   *
   * values and accept their updates only if in range.                */
  int x = INT_MAX, y = INT_MAX;
  
  world.cur_map->cmap[world.pc.pos[dim_y]][world.pc.pos[dim_x]] = NULL;

  echo();
  curs_set(1);
  do {
    mvprintw(0, 0, "Enter x [-200, 200]:           ");
    refresh();
    mvscanw(0, 21, (char *) "%d", &x);
  } while (x < -200 || x > 200);
  do {
    mvprintw(0, 0, "Enter y [-200, 200]:          ");
    refresh();
    mvscanw(0, 21, (char *) "%d", &y);
  } while (y < -200 || y > 200);

  refresh();
  noecho();
  curs_set(0);

  x += 200;
  y += 200;

  world.cur_idx[dim_x] = x;
  world.cur_idx[dim_y] = y;

  new_map(1);
  io_teleport_pc(dest);
}

void io_bag() {
  clear();
  mvprintw(2,0,"1: Potions x%d",world.pc.potions);
  mvprintw(3,0,"2: Revives x%d", world.pc.revives);
  mvprintw(4,0,"3: Pokeballs x%d",world.pc.pokeballs);
  mvprintw(5,0,"4: Exit");

  int x = -1;
  echo();
  curs_set(1);
  do {
    mvprintw(0, 0, "Which item would you like to use?: ");
    int y = getcurx(stdscr);
    mvscanw(0, y, (char *) "%d", &x);
  } while (x < 1 || x > 4);
  
  if (x == 1 && world.pc.potions <= 0) {
    mvprintw(7,0,"You are out of potions, go to the pokemart for more.");
    getch();
  }

  else if (x == 1) {
    int i = 1; int r = 9;
    for (auto &p : world.pc.lineup) {
      if (!p.fainted) {
        mvprintw(r++,0,"%d: %s HP:%d/%d",i,pokemon[p.randomPokemonIndex].identifier,p.HP,p.stats[0]);
      }
      i++;
    }
    
    int pick_to_heal = -2;
    do {
      if (pick_to_heal != -2) mvprintw(8,0,"Invalid Input!");
      mvprintw(7,0,"Who would you like to use the potion on? (-1 to exit): ");
      int y = getcurx(stdscr);
      mvscanw(7, y, (char *) "%d", &pick_to_heal);
    } while (pick_to_heal != -1 && (world.pc.lineup[pick_to_heal-1].fainted || pick_to_heal < 1 || pick_to_heal > (int)world.pc.lineup.size()));

    if (pick_to_heal != -1) {
      move(8,0);
      clrtoeol();
      int old_hp = world.pc.lineup[pick_to_heal-1].HP;
      int new_hp = world.pc.lineup[pick_to_heal-1].HP + 20 > world.pc.lineup[pick_to_heal-1].stats[0] ? world.pc.lineup[pick_to_heal-1].stats[0] : world.pc.lineup[pick_to_heal-1].HP + 20;
      int healed = new_hp - old_hp;
      
      world.pc.lineup[pick_to_heal-1].HP = new_hp;

      r = 9;
      for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
        move(r++,0);
        clrtoeol();
      }
      mvprintw(8,0,"%s healed %d HP from the potion.",pokemon[world.pc.lineup[pick_to_heal-1].randomPokemonIndex].identifier,healed);
      world.pc.potions--;
      getch();
    }
  }

  // revive
  if (x == 2 && world.pc.revives <= 0) {
    mvprintw(7,0,"You are out of revives, go to the pokemart for more.");
    getch();
  }
  else if (x == 2) {
    int i = 1; int r = 9;
    for (auto &p : world.pc.lineup) {
      if (p.fainted)
        mvprintw(r++,0,"%d: %s HP:%d/%d",i,pokemon[p.randomPokemonIndex].identifier,p.HP,p.stats[0]);
      i++;
    }
    
    int pick_to_heal = -2;
    do {
      if (pick_to_heal != -2) mvprintw(8,0,"Invalid Input!");
      mvprintw(7,0,"Who would you like to use the revive on? (-1 to exit): ");
      int y = getcurx(stdscr);
      mvscanw(7, y, (char *) "%d", &pick_to_heal);
    } while (pick_to_heal != -1 && (!world.pc.lineup[pick_to_heal-1].fainted || pick_to_heal < 1 || pick_to_heal > (int)world.pc.lineup.size()));

    if (pick_to_heal != -1) {
      move(8,0);
      clrtoeol();
      
      r = 9;
      for (int i = 0; i < (int)world.pc.lineup.size(); i++) {
        move(r++,0);
        clrtoeol();
      }

      world.pc.lineup[pick_to_heal-1].HP = world.pc.lineup[pick_to_heal-1].stats[0]/2;
      world.pc.lineup[pick_to_heal-1].fainted = 0;

      mvprintw(8,0,"%s was revived.",pokemon[world.pc.lineup[pick_to_heal-1].randomPokemonIndex].identifier);
      world.pc.revives--;
      getch();
    }
  }

  if (x == 3) {
    mvprintw(7,0,"Pokeballs can only be used in battles.");
    getch();
  }

  refresh();
  noecho();
  curs_set(0);

  io_display();
  refresh();
}

void io_handle_input(pair_t dest)
{
  uint32_t turn_not_consumed;
  int key;
  do {
    switch (key = getch()) {
    case '7':
    case 'y':
    case KEY_HOME:
      turn_not_consumed = move_pc_dir(7, dest);
      break;
    case '8':
    case 'k':
    case KEY_UP:
      turn_not_consumed = move_pc_dir(8, dest);
      break;
    case '9':
    case 'u':
    case KEY_PPAGE:
      turn_not_consumed = move_pc_dir(9, dest);
      break;
    case '6':
    case 'l':
    case KEY_RIGHT:
      turn_not_consumed = move_pc_dir(6, dest);
      break;
    case '3':
    case 'n':
    case KEY_NPAGE:
      turn_not_consumed = move_pc_dir(3, dest);
      break;
    case '2':
    case 'j':
    case KEY_DOWN:
      turn_not_consumed = move_pc_dir(2, dest);
      break;
    case '1':
    case 'b':
      io_bag();
      turn_not_consumed = 1;
      break;
    case KEY_END:
      turn_not_consumed = move_pc_dir(1, dest);
      break;
    case '4':
    case 'h':
    case KEY_LEFT:
      turn_not_consumed = move_pc_dir(4, dest);
      break;
    case '5':
    case ' ':
    case '.':
    case KEY_B2:
      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      turn_not_consumed = 0;
      break;
    case '>':
      turn_not_consumed = move_pc_dir('>', dest);
      break;
    case 'Q':
      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      world.quit = 1;
      turn_not_consumed = 0;
      break;
      break;
    case 't':
      io_list_trainers();
      turn_not_consumed = 1;
      break;
    case 'p':
      /* Teleport the PC to a random place in the map.              */
      io_teleport_pc(dest);
      turn_not_consumed = 0;
      break;
   case 'f':
      /* Fly to any map in the world.                                */
      io_teleport_world(dest);
      turn_not_consumed = 0;
      break;    
    case 'q':
      /* Demonstrate use of the message queue.  You can use this for *
       * printf()-style debugging (though gdb is probably a better   *
       * option.  Not that it matters, but using this command will   *
       * waste a turn.  Set turn_not_consumed to 1 and you should be *
       * able to figure out why I did it that way.                   */
      io_queue_message("This is the first message.");
      io_queue_message("Since there are multiple messages, "
                       "you will see \"more\" prompts.");
      io_queue_message("You can use any key to advance through messages.");
      io_queue_message("Normal gameplay will not resume until the queue "
                       "is empty.");
      io_queue_message("Long lines will be truncated, not wrapped.");
      io_queue_message("io_queue_message() is variadic and handles "
                       "all printf() conversion specifiers.");
      io_queue_message("Did you see %s?", "what I did there");
      io_queue_message("When the last message is displayed, there will "
                       "be no \"more\" prompt.");
      io_queue_message("Have fun!  And happy printing!");
      io_queue_message("Oh!  And use 'Q' to quit!");

      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      turn_not_consumed = 0;
      break;
    default:
      /* Also not in the spec.  It's not always easy to figure out what *
       * key code corresponds with a given keystroke.  Print out any    *
       * unhandled key here.  Not only does it give a visual error      *
       * indicator, but it also gives an integer value that can be used *
       * for that key in this (or other) switch statements.  Printed in *
       * octal, with the leading zero, because ncurses.h lists codes in *
       * octal, thus allowing us to do reverse lookups.  If a key has a *
       * name defined in the header, you can use the name here, else    *
       * you can directly use the octal value.                          */
      mvprintw(0, 0, "Unbound key: %#o ", key);
      turn_not_consumed = 1;
    }
    refresh();
  } while (turn_not_consumed);
}
