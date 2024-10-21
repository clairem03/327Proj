#ifndef IO_H
# define IO_H

#include <random>

class character;
typedef int16_t pair_t[2];

class random_pokemon {
  public:
  int randomPokemonIndex;
  int level;
  std::vector<int> move_ids;
  std::vector<int> base_stats;
  std::vector<int> IVs;
  std::vector<int> stats;
  int HP; // curr hp may change in battle
  int gender; // 0 == male, 1 == female
  int is_shiny; // 1/8192
  int fainted;

  random_pokemon() 
    : randomPokemonIndex((rand() % 1092) + 1), level(setLevel()), move_ids(setMoves()),
      base_stats(getBaseStats()), IVs(genIVs()), stats(calculateStats()), HP(stats[0]),
      gender(rand() % 2), is_shiny(rand() % 10 == 0), fainted(0)
    {}

  
  int setLevel();

  std::vector<int> setMoves();

  std::vector<int> getBaseStats();

  std::vector<int> genIVs();

  std::vector<int> calculateStats();

};

void io_init_terminal(void);
void io_reset_terminal(void);
void io_display(void);
void io_handle_input(pair_t dest);
void io_queue_message(const char *format, ...);
void io_battle(character *aggressor, character *defender);
void io_encounter();
void io_starter();

#endif
