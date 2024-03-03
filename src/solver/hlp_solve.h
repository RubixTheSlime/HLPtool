#ifndef HLP_SOLVE
#define HLP_SOLVE
#include "../arg_global.h"
#include "../redstone.h"
#include <stdint.h>

enum search_accuracy { ACCURACY_REDUCED=-1, ACCURACY_NORMAL, ACCURACY_INCREASED, ACCURACY_PERFECT };
enum solve_config_error { HLP_ERROR_BLANK=1, HLP_ERROR_NULL, HLP_ERROR_MALFORMED, HLP_ERROR_TOO_LONG };
enum hlp_solve_type { HLP_SOLVE_TYPE_EXACT, HLP_SOLVE_TYPE_PARTIAL, HLP_SOLVE_TYPE_RANGED };

struct hlp_request {
    uint64_t mins;
    uint64_t maxs;
    enum hlp_solve_type solve_type;
    enum solve_config_error error;
};

// the start position, or at least the pretty one that can be used outside the solver
extern const uint64_t hlp_start_pos;

extern const uint64_t broadcast_h16; // 0x1111...

extern int hlp_solve_verbosity;

/* search for a solution for the given map
 * returns length of chain
 */
int solve(struct hlp_request request, uint16_t* output_chain, int max_depth, enum search_accuracy accuracy);

/* parse the string into a solve request
 */
struct hlp_request parse_hlp_request_str(char* str);

void print_hlp_request(struct hlp_request request);

void print_hlp_map(uint64_t map);

// access the layer information
uint16_t get_layer_conf(int group, int layer_id);
uint16_t get_next_valid_layer_id(int group, int prev_layer_id, int index);
uint16_t get_next_valid_layer_size(int group, int layer_id);

void hlp_print_search(char* map);


uint64_t apply_chain(uint64_t start, uint16_t* chain, int length);

void print_chain(uint16_t* chain, int length);

struct arg_settings_solver_hex {
    struct arg_settings_global* global;
    struct arg_settings_redstone settings_redstone;
};

extern struct argp argp_solver_hex;

#endif
