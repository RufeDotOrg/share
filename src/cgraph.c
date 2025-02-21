// Rufe.org LLC 2022-2025: ISC License
#include "game.c"

// platform layer?
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

enum { MAX_FUNC = 8 * 1024 };  // skip asm with more than X functions
enum { MAX_CALL = 512 };       // skip children when more than X calls
enum { MAX_NAME = 64 };        // truncate function name longer than X
DATA char funcD[MAX_FUNC][MAX_NAME];
DATA uint64_t func_lineD[MAX_FUNC];
DATA uint64_t func_addrD[MAX_FUNC];
DATA int func_ccD[MAX_FUNC];
DATA int func_depthD[MAX_FUNC];
DATA int func_stackD[MAX_FUNC];
DATA int func_recurseD[MAX_FUNC];
DATA int64_t call_addrD[MAX_FUNC][MAX_CALL];

// Post processing
enum { DEPTH_SORT = 1 };     // determine callstack depth
enum { CHECK_SINGLE = 1 };   // argv[2] determine single callstack
enum { DEBUG_CALL = 0 };     // log function calls during parse
enum { VERBOSE_DEPTH = 0 };  // log function depth
enum { RM_SELFCALL = 1 };    // remove self-calls for examination of graph flow
enum { VERBOSE_SELFCALL = 0 };  // log changes by break cycle

int
func_name(id, name)
char* name;
{
  int it = 0;
  for (; it < MAX_NAME - 1; ++it) {
    if (*name == '>') break;

    funcD[id][it] = *name;
    name += 1;
  }
  funcD[id][it] = 0;
  return it;
}
int
find_name(name)
char* name;
{
  for (int id = 0; id < MAX_FUNC; ++id) {
    if (strncmp(funcD[id], name, MAX_NAME) == 0) return id;
  }
  return MAX_FUNC;
}
int
func_cc(id)
{
  return func_ccD[id + 1] - func_ccD[id];
}
// Includes pad between functions; may vary slightly from nm
int
func_size(id)
{
  return func_addrD[id + 1] - func_addrD[id];
}
int
func_info(id)
{
  printf("%d) %s %p depth%c %d (0x%x bytecode size)\n", id, funcD[id],
         (void*)func_addrD[id], func_recurseD[id] ? '*' : ' ', func_depthD[id],
         func_size(id));
  return 0;
}
int
is_call(char* str)
{
  return (0 == memcmp(str, S2("call")));
}
int
func_addr(int func, int addr)
{
  int lo = 0;
  int hi = func - 1;
  while (lo <= hi) {
    int idx = (lo + hi) / 2;
    if (func_addrD[idx] == addr) return idx;
    if (func_addrD[idx] > addr) {
      hi = idx - 1;
    }
    if (func_addrD[idx] < addr) {
      lo = idx + 1;
    }
  }
  return -1;
}

int
anydepth_check_func(int id, int func)
{
  if (func_depthD[id]) return func_depthD[id];
  if (func_stackD[id]) {
    func_recurseD[id] = 1;
    if (VERBOSE_DEPTH) printf("recursion ");
    if (VERBOSE_DEPTH) func_info(id);
    return -1;
  }

  int call_count = func_cc(id);
  int depth = 1;
  func_stackD[id]++;
  for (int it = 0; it < call_count; ++it) {
    int call_addr = call_addrD[id][it];
    int call_id = func_addr(func, call_addr);

    int call_depth = 1;
    if (call_id >= 0) call_depth = 1 + anydepth_check_func(call_id, func);
    if (!call_depth) depth = 0;
    if (!call_depth) break;
    ST_MAX(depth, call_depth);
  }
  func_stackD[id]--;

  if (!depth) return 0;
  func_depthD[id] = depth;

  if (VERBOSE_DEPTH) printf("resolved ");
  if (VERBOSE_DEPTH) func_info(id);

  return depth;
}
int
cmp(void* a, void* b)
{
  return *iptr(a) - *iptr(b);
}
static char* progD;
int
usage()
{
  printf("%s <asm> [single_symbol]\n", progD);
  printf("\n");
  printf("  use `objdump -Mintel -d <binary>` to generate <asm>\n");
  printf("\nasm prefixes\n");
  printf("?: sanity check\n");
  printf("\nsingle symbol prefixes:\n");
  printf("+: list parents\n");
  printf("-: list children\n");
  printf("%%: show functions not called by symbol (inverse)\n");
  printf("=: depth sort\n");
  return 10;
}
int
main(int argc, char** argv)
{
  if (argc > 0) progD = argv[0];
  if (argc <= 1) return usage();

  char* filename = argv[1];
  int opt_scan = argc > 1 && argv[1][0] == '?';
  filename += (opt_scan != 0);
  printf("open %s\n", filename);
  int fd = open(filename, O_RDONLY);
  if (fd == -1) return 2;

  struct stat sb;
  if (fstat(fd, &sb) != 0) return 2;
  uint64_t byte_count = sb.st_size;
  printf("%ju byte_count\n", byte_count);
  void* buffer = mmap(0, byte_count, PROT_READ, MAP_PRIVATE, fd, 0);
  printf("mmap %p\n", buffer);
  close(fd);

  char* iter = buffer;
  int line = 0;
  int func = 0;
  int call_count = 0;
  uint64_t lo_addr = UINT64_MAX;
  uint64_t hi_addr = 0;
  while (iter) {
    char* end = strchr(iter, '\n');
    end += (end != 0);

    if (char_digit(*iter)) {
      int64_t addr = strtoull(iter, 0, 16);
      // cosmo .init.202.ifunc at 0x00
      if (addr) ST_MIN(lo_addr, addr);
      ST_MAX(hi_addr, addr);

      func_addrD[func] = addr;
      func_lineD[func] = line;
      enum { NAME_OF = 18 };
      iter += NAME_OF;
      func_name(func, iter);

      func_ccD[func] = call_count;
      if (DEBUG_CALL)
        printf("%d) %s %p %d\n", func, funcD[func], (void*)addr, call_count);
      func += 1;

    } else {
      enum { CALL_OF = 40 };
      if (end - iter > CALL_OF && is_call(&iter[CALL_OF])) {
        int this_id = func - 1;
        int call_base = func_ccD[this_id];
        int call_idx = call_count - call_base;

        enum { CALL_ADDR_OF = 45 };
        int64_t call_addr = strtoull(&iter[CALL_ADDR_OF], 0, 16);
        if (DEBUG_CALL)
          printf("%.20s | 0x%jx call_addr \n", &iter[CALL_OF], call_addr);
        // TBD: call_addr 0: function pointer
        // TBD: call_addr < 0: stack call
        if (call_idx < MAX_CALL) call_addrD[this_id][call_idx] = call_addr;
        if (call_idx < MAX_CALL) call_count += 1;
      }
    }

    line += 1;
    iter = end;
  }
  func_ccD[func] = call_count;

  printf("%d line count\n", line);
  printf("%d func count\n", func);
  printf("%d call_count\n", call_count);
  printf("0x%jx-0x%jx [lo, hi] address\n", lo_addr, hi_addr);
  printf("\n");

  if (opt_scan) {
    printf("scan for unknown\n");
    int hival = 0;
    int hi_idx = -1;
    for (int it = 0; it < func; ++it) {
      int call_count = func_cc(it);
      for (int jt = 0; jt < call_count; ++jt) {
        int64_t call_addr = call_addrD[it][jt];
        int call_id = func_addr(func, call_addr);
        int show = (call_id < 0 && call_addr > 0);
        if (show) printf("%d) %s %d ", it, funcD[it], call_count);
        if (show) printf("  %p %d\n", (void*)call_addr, call_id);
      }

      if (call_count < MAX_CALL) {
        if (call_count > hival) hi_idx = it;
        ST_MAX(hival, call_count);
      }
    }

    printf("\n");
    for (int it = 0; it < func; ++it) {
      if (func_cc(it) == MAX_CALL) {
        printf("(%d) !%s call may be in excess of MAX_CALL\n", it, funcD[it]);
      }
    }

    if (hi_idx > 0)
      printf("%d/%d call_count hival (%s)\n", hival, MAX_CALL, funcD[hi_idx]);
    exit(0);
  }

  int parent = argc > 2 && argv[2][0] == '+';
  if (parent) {
    int id = find_name(&argv[2][1]);
    // int it = strtoull(argv[2], 0, 10);
    if (id < func) {
      printf("\nlist parents ");
      func_info(id);
      int func_addr = func_addrD[id];
      for (int it = 0; it < func; ++it) {
        int call_count = func_cc(it);
        for (int jt = 0; jt < call_count; ++jt) {
          if (call_addrD[it][jt] == func_addr) {
            printf("called by %s\n", funcD[it]);
            jt = call_count;
          }
        }
      }
    }
    exit(0);
  }

  int child = argc > 2 && argv[2][0] == '-';
  if (child) {
    int id = find_name(&argv[2][1]);
    // int it = strtoull(argv[2], 0, 10);
    if (id < func) {
      printf("\nlist children ");
      func_info(id);
      int call_count = func_cc(id);
      for (int jt = 0; jt < call_count; ++jt) {
        int call_addr = call_addrD[id][jt];
        int call_id = func_addr(func, call_addr);
        if (call_id >= 0) printf("  %s\n", funcD[call_id]);
      }
    }
    exit(0);
  }

  if (RM_SELFCALL) {
    if (VERBOSE_SELFCALL) printf("\n");
    for (int id = 0; id < func; ++id) {
      int call_count = func_cc(id);
      int last_call = -1;
      for (int jt = 0; jt < call_count; ++jt) {
        int64_t call_addr = call_addrD[id][jt];
        int call_id = func_addr(func, call_addr);
        if (call_id == id) call_addrD[id][jt] = func_addrD[0];

        if (call_id == id && call_id != last_call) {
          last_call = id;
          if (VERBOSE_SELFCALL) printf("selfcall ");
          if (VERBOSE_SELFCALL) func_info(id);
        }
      }
    }
    if (VERBOSE_SELFCALL) printf("\n");
  }

  if (CHECK_SINGLE && argc > 2) {
    int show_unused = argc > 2 && argv[2][0] == '%';
    int show_sorted = argc > 2 && argv[2][0] == '=';
    char* name = argv[2] + (show_unused || show_sorted);
    int it = find_name(name);

    // int it = strtoull(argv[2], 0, 10);
    if (it < func) {
      int depth = anydepth_check_func(it, func);

      int64_t treesize = 0;
      for (int it = 0; it < func; ++it) {
        if (func_depthD[it]) treesize += func_size(it);
      }
      printf("analysis ");
      func_info(it);
      printf("%jd [0x%jx] treesize of bytecode\n", treesize, treesize);
      printf("%.04f codebase %jd\n", (float)treesize / (hi_addr - lo_addr),
             hi_addr - lo_addr);

      if (show_unused) {
        printf("\n");
        int count = 0;
        for (int it = 0; it < func; ++it) {
          if (!func_depthD[it]) count += 1;
          if (!func_depthD[it]) func_info(it);
        }
        printf("\n%d+%d = %d unused/used functions\n", count, func - count,
               func);
      }

      if (show_sorted) {
        printf("\n");
        uint64_t blob[MAX_FUNC];
        for (int it = 0; it < func; ++it) {
          blob[it] = ((uint64_t)it << 32) | func_depthD[it];
        }
        qsort(blob, func, sizeof(uint64_t), cmp);
        for (int it = 0; it < func; ++it) {
          int* pair = vptr(&blob[it]);
          int depth = *iptr(pair);
          int id = *iptr(pair + 1);
          if (depth) func_info(id);
        }
        printf("* recursive function\n");
      }
    }
    exit(0);
  }

  if (DEPTH_SORT) {
    int hi_depth = 0;
    for (int it = 0; it < func; ++it) {
      int depth = anydepth_check_func(it, func);
      ST_MAX(hi_depth, depth);
    }

    printf("\nhi_depth %d\n", hi_depth);
    uint64_t blob[MAX_FUNC];
    for (int it = 0; it < func; ++it) {
      blob[it] = ((uint64_t)it << 32) | func_depthD[it];
    }
    qsort(blob, func, sizeof(uint64_t), cmp);
    for (int it = 0; it < func; ++it) {
      int* pair = vptr(&blob[it]);
      int depth = *iptr(pair);
      int id = *iptr(pair + 1);
      printf("%d) %s depth%c %d (0x%x bytecode size)\n", id, funcD[id],
             func_recurseD[id] ? '*' : ' ', depth, func_size(id));
    }
    printf("* recursive function\n");
  }

  return 0;
}
