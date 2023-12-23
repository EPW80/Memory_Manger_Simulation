//  mem_mgr.cpp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <cassert>

#pragma warning(disable: 4996)

#define ARGC_ERROR 1
#define FILE_ERROR 2

#define FRAME_SIZE 256
#define FIFO 0
#define LRU 1
#define REPLACE_POLICY FIFO

// SET TO 128 to use replacement policy: FIFO or LRU,
#define NFRAMES 256
#define PTABLE_SIZE 256
#define TLB_SIZE 16

struct page_node {
  size_t npage;
  size_t frame_num;
  bool is_present;
  bool is_used;

};

bool free_frames[NFRAMES];

char * ram = (char * ) malloc(NFRAMES * FRAME_SIZE);
page_node pg_table[PTABLE_SIZE]; // page table and (single) TLB
page_node tlb[TLB_SIZE];

const char * passed_or_failed(bool condition) {
  return condition ? " + " : "fail";
}
size_t failed_asserts = 0;

size_t get_page(size_t x) {
  return 0xff & (x >> 8);
}
size_t get_offset(size_t x) {
  return 0xff & x;
}

void get_page_offset(size_t x, size_t & page, size_t & offset) {
  page = get_page(x);
  offset = get_offset(x);
  // printf("x is: %zu, page: %zu, offset: %zu, address: %zu, paddress: %zu\n",
  //        x, page, offset, (page << 8) | get_offset(x), page * 256 + offset);
}

void update_frame_ptable(size_t npage, size_t frame_num) {
  pg_table[npage].frame_num = frame_num;
  pg_table[npage].is_present = true;
  pg_table[npage].is_used = true;
}

int find_frame_ptable(size_t frame) { // FIFO
  for (int i = 0; i < PTABLE_SIZE; i++) {
    if (pg_table[i].frame_num == frame &&
      pg_table[i].is_present == true) {
      return i;
    }
  }
  return -1;
}

size_t get_used_ptable() { // LRU
  size_t unused = -1;
  for (size_t i = 0; i < PTABLE_SIZE; i++) {
    if (pg_table[i].is_used == false &&
      pg_table[i].is_present == true) {
      return (size_t) i;
    }
  }
  // All present pages have been used recently, set all page entry used flags to false
  for (size_t i = 0; i < PTABLE_SIZE; i++) {
    pg_table[i].is_used = false;
  }
  for (size_t i = 0; i < PTABLE_SIZE; i++) {
    page_node & r = pg_table[i];
    if (!r.is_used && r.is_present) {
      return i;
    }
  }
  return (size_t) - 1;
}

int check_tlb(size_t page) {
  for (int i = 0; i < TLB_SIZE; i++) {
    if (tlb[i].npage == page) {
      return i;
    }
  }
  return -1;
}

void open_files(FILE * & fadd, FILE * & fcorr, FILE * & fback) {
  fadd = fopen("addresses.txt", "r");
  if (fadd == NULL) {
    fprintf(stderr, "Could not open file: 'addresses.txt'\n");
    exit(FILE_ERROR);
  }

  fcorr = fopen("correct.txt", "r");
  if (fcorr == NULL) {
    fprintf(stderr, "Could not open file: 'correct.txt'\n");
    exit(FILE_ERROR);
  }

  fback = fopen("BACKING_STORE.bin", "rb");
  if (fback == NULL) {
    fprintf(stderr, "Could not open file: 'BACKING_STORE.bin'\n");
    exit(FILE_ERROR);
  }
}
void close_files(FILE * fadd, FILE * fcorr, FILE * fback) {
  fclose(fadd);
  fclose(fcorr);
  fclose(fback);
}

void initialize_pg_table_tlb() {
  for (int i = 0; i < PTABLE_SIZE; ++i) {
    pg_table[i].npage = i;
    pg_table[i].is_present = false;
    pg_table[i].is_used = false;
  }
  for (int i = 0; i < NFRAMES; ++i) {
    free_frames[i] = true; // All frames are initially free
  }
  for (int i = 0; i < TLB_SIZE; ++i) {
    tlb[i].npage = (size_t) - 1;
    tlb[i].is_present = false;
  }
}

void summarize(size_t pg_faults, size_t tlb_hits) {
  printf("\nPage Fault Percentage: %1.3f%%", (double) pg_faults / 1000);
  printf("\nTLB Hit Percentage: %1.3f%%\n\n", (double) tlb_hits / 1000);
  printf("ALL logical ---> physical assertions PASSED!\n");
  printf("\n\t\t...done.\n");
}

void tlb_add(int index, page_node entry) {
  tlb[index] = entry;
} // TODO

void tlb_remove(int index) {
  // Invalidate TLB entry
  tlb[index].npage = (size_t) - 1;
  tlb[index].is_present = false;
} // TODO

void tlb_hit(size_t & frame, size_t & page, size_t & tlb_hits, int result) {
  frame = tlb[result].frame_num;
  tlb_hits++; // increment TLB hit counter
} // TODO

void tlb_miss(size_t& frame, size_t& page, size_t& tlb_track) {
    // Frame is taken from the page table
    frame = pg_table[page].frame_num;
    // Update TLB with new entry
    tlb_add(tlb_track, pg_table[page]);
    // Increment TLB track for the next entry
    tlb_track = (tlb_track + 1) % TLB_SIZE;
} // TODO

void fifo_replace_page(size_t& frame) {
    static size_t next_replacement = 0;
    // Find the frame that is next in line for replacement
    frame = next_replacement;
    // Invalidate the page table entry of the replaced frame
    int replaced_page = find_frame_ptable(frame);
    if (replaced_page != -1) {
        pg_table[replaced_page].is_present = false;
    }
    // Update next replacement frame
    next_replacement = (next_replacement + 1) % NFRAMES;
} // TODO

void lru_replace_page(size_t& frame) {
    // Find the least recently used frame
    size_t lru_page_index = get_used_ptable();
    frame = pg_table[lru_page_index].frame_num;
    // Invalidate the page table entry
    pg_table[lru_page_index].is_present = false;
}
// TODO

void page_fault(size_t& frame, size_t& page, size_t& frames_used, size_t& pg_faults, 
              size_t& tlb_track, FILE* fbacking) {  
    unsigned char buf[FRAME_SIZE];
    memset(buf, 0, sizeof(buf));
    bool is_memfull = frames_used >= NFRAMES;

    ++pg_faults; // Increment the page fault count

    if (is_memfull) {
        // Select a frame to replace based on the policy
        if (REPLACE_POLICY == FIFO) {
            fifo_replace_page(frame);
        } else { // LRU policy
            lru_replace_page(frame);
        }
    } else {
        // Find the next available frame
        frame = frames_used % NFRAMES;
        frames_used++; // Increment the number of frames used
    }

    // Load the page into RAM
    fseek(fbacking, page * FRAME_SIZE, SEEK_SET);
    fread(buf, FRAME_SIZE, 1, fbacking);

    // Copy the loaded page into the frame in RAM
    memcpy(ram + frame * FRAME_SIZE, buf, FRAME_SIZE);

    // Update the page table with the new frame information
    update_frame_ptable(page, frame);

    // Update the TLB
    tlb_add(tlb_track, pg_table[page]);
    tlb_track = (tlb_track + 1) % TLB_SIZE;
}


void check_address_value(size_t logic_add, size_t page, size_t offset, size_t physical_add,
  size_t & prev_frame, size_t frame, int val, int value, size_t o) {
  printf("log: %5lu 0x%04x (pg:%3lu, off:%3lu)-->phy: %5lu (frm: %3lu) (prv: %3lu)--> val: %4d == value: %4d -- %s",
    logic_add, (unsigned int) logic_add, page, offset, physical_add, frame, prev_frame,
    val, value, passed_or_failed(val == value));

  if (frame < prev_frame) {
    printf("   HIT!\n");
  } else {
    prev_frame = frame;
    printf("----> pg_fault\n");
  }
  if (o % 5 == 4) {
    printf("\n");
  }
  // if (o > 20) { exit(-1); }             // to check out first 20 elements

  if (val != value) {
    ++failed_asserts;
  }
  if (failed_asserts > 5) {
    exit(-1);
  }
  //     assert(val == value);
}

void run_simulation() {
  // addresses, pages, frames, values, hits and faults
  size_t logic_add, virt_add, phys_add, physical_add;
  size_t page, frame, offset, value, prev_frame = 0, tlb_track = 0;
  size_t frames_used = 0, pg_faults = 0, tlb_hits = 0;
  int val = 0;
  char buf[BUFSIZ];

  bool is_memfull = false; // physical memory to store the frames

  initialize_pg_table_tlb();

  // addresses to test, correct values, and pages to load
  FILE * faddress, * fcorrect, * fbacking;
  open_files(faddress, fcorrect, fbacking);

  for (int o = 0; o < 1000; o++) { // read from file correct.txt
    fscanf(fcorrect, "%s %s %lu %s %s %lu %s %ld", buf, buf, & virt_add, buf, buf, & phys_add, buf, & value);

    fscanf(faddress, "%ld", & logic_add);
    get_page_offset(logic_add, page, offset);

    int result = check_tlb(page);
    if (result >= 0) {
      tlb_hit(frame, page, tlb_hits, result);
    } else if (pg_table[page].is_present) {
      tlb_miss(frame, page, tlb_track);
    } else { // page fault
      page_fault(frame, page, frames_used, pg_faults, tlb_track, fbacking);
    }

    physical_add = (frame * FRAME_SIZE) + offset;
    val = (int) * (ram + physical_add);

    check_address_value(logic_add, page, offset, physical_add, prev_frame, frame, val, value, o);
  }
  close_files(faddress, fcorrect, fbacking); // and time to wrap things up
  free(ram);
  summarize(pg_faults, tlb_hits);
}

int main(int argc,
  const char * argv[]) {
  run_simulation();
  // printf("\nFailed asserts: %lu\n\n", failed_asserts);   // allows asserts to fail silently and be counted
  return 0;
}

// output to console: addresses.txt

// log: 50352 0xc4b0 (pg:196, off:176)-->phy: 14000 (frm:  54) (prv: 122)--> val:    0 == value:    0 --  +    HIT!
// log: 49737 0xc249 (pg:194, off: 73)-->phy: 31561 (frm: 123) (prv: 122)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 15555 0x3cc3 (pg: 60, off:195)-->phy: 31939 (frm: 124) (prv: 123)--> val:   48 == value:   48 --  + ----> pg_fault
// log: 47475 0xb973 (pg:185, off:115)-->phy: 32115 (frm: 125) (prv: 124)--> val:   92 == value:   92 --  + ----> pg_fault
// log: 15328 0x3be0 (pg: 59, off:224)-->phy: 32480 (frm: 126) (prv: 125)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 34621 0x873d (pg:135, off: 61)-->phy: 17981 (frm:  70) (prv: 126)--> val:    0 == value:    0 --  +    HIT!
// log: 51365 0xc8a5 (pg:200, off:165)-->phy: 32677 (frm: 127) (prv: 126)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 32820 0x8034 (pg:128, off: 52)-->phy:  4148 (frm:  16) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 48855 0xbed7 (pg:190, off:215)-->phy: 32983 (frm: 128) (prv: 127)--> val:  -75 == value:  -75 --  + ----> pg_fault
// log: 12224 0x2fc0 (pg: 47, off:192)-->phy:  2752 (frm:  10) (prv: 128)--> val:    0 == value:    0 --  +    HIT!
// log:  2035 0x07f3 (pg:  7, off:243)-->phy: 33267 (frm: 129) (prv: 128)--> val:   -4 == value:   -4 --  + ----> pg_fault
// log: 60539 0xec7b (pg:236, off:123)-->phy:  7291 (frm:  28) (prv: 129)--> val:   30 == value:   30 --  +    HIT!

// log: 14595 0x3903 (pg: 57, off:  3)-->phy: 33283 (frm: 130) (prv: 129)--> val:   64 == value:   64 --  + ----> pg_fault
// log: 13853 0x361d (pg: 54, off: 29)-->phy: 31261 (frm: 122) (prv: 130)--> val:    0 == value:    0 --  +    HIT!
// log: 24143 0x5e4f (pg: 94, off: 79)-->phy: 33615 (frm: 131) (prv: 130)--> val: -109 == value: -109 --  + ----> pg_fault
// log: 15216 0x3b70 (pg: 59, off:112)-->phy: 32368 (frm: 126) (prv: 131)--> val:    0 == value:    0 --  +    HIT!
// log:  8113 0x1fb1 (pg: 31, off:177)-->phy: 33969 (frm: 132) (prv: 131)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 22640 0x5870 (pg: 88, off:112)-->phy:  2928 (frm:  11) (prv: 132)--> val:    0 == value:    0 --  +    HIT!
// log: 32978 0x80d2 (pg:128, off:210)-->phy:  4306 (frm:  16) (prv: 132)--> val:   32 == value:   32 --  +    HIT!
// log: 39151 0x98ef (pg:152, off:239)-->phy:  4079 (frm:  15) (prv: 132)--> val:   59 == value:   59 --  +    HIT!
// log: 19520 0x4c40 (pg: 76, off: 64)-->phy: 22592 (frm:  88) (prv: 132)--> val:    0 == value:    0 --  +    HIT!
// log: 58141 0xe31d (pg:227, off: 29)-->phy: 34077 (frm: 133) (prv: 132)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 63959 0xf9d7 (pg:249, off:215)-->phy: 21975 (frm:  85) (prv: 133)--> val:  117 == value:  117 --  +    HIT!
// log: 53040 0xcf30 (pg:207, off: 48)-->phy: 34352 (frm: 134) (prv: 133)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 55842 0xda22 (pg:218, off: 34)-->phy: 34594 (frm: 135) (prv: 134)--> val:   54 == value:   54 --  + ----> pg_fault
// log:   585 0x0249 (pg:  2, off: 73)-->phy: 17225 (frm:  67) (prv: 135)--> val:    0 == value:    0 --  +    HIT!
// log: 51229 0xc81d (pg:200, off: 29)-->phy: 32541 (frm: 127) (prv: 135)--> val:    0 == value:    0 --  +    HIT!

// log: 64181 0xfab5 (pg:250, off:181)-->phy:  4533 (frm:  17) (prv: 135)--> val:    0 == value:    0 --  +    HIT!
// log: 54879 0xd65f (pg:214, off: 95)-->phy:  3679 (frm:  14) (prv: 135)--> val: -105 == value: -105 --  +    HIT!
// log: 28210 0x6e32 (pg:110, off: 50)-->phy: 34866 (frm: 136) (prv: 135)--> val:   27 == value:   27 --  + ----> pg_fault
// log: 10268 0x281c (pg: 40, off: 28)-->phy: 14620 (frm:  57) (prv: 136)--> val:    0 == value:    0 --  +    HIT!
// log: 15395 0x3c23 (pg: 60, off: 35)-->phy: 31779 (frm: 124) (prv: 136)--> val:    8 == value:    8 --  +    HIT!

// log: 12884 0x3254 (pg: 50, off: 84)-->phy: 30292 (frm: 118) (prv: 136)--> val:    0 == value:    0 --  +    HIT!
// log:  2149 0x0865 (pg:  8, off:101)-->phy: 35173 (frm: 137) (prv: 136)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 53483 0xd0eb (pg:208, off:235)-->phy: 35563 (frm: 138) (prv: 137)--> val:   58 == value:   58 --  + ----> pg_fault
// log: 59606 0xe8d6 (pg:232, off:214)-->phy: 26070 (frm: 101) (prv: 138)--> val:   58 == value:   58 --  +    HIT!
// log: 14981 0x3a85 (pg: 58, off:133)-->phy: 24709 (frm:  96) (prv: 138)--> val:    0 == value:    0 --  +    HIT!

// log: 36672 0x8f40 (pg:143, off: 64)-->phy: 35648 (frm: 139) (prv: 138)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 23197 0x5a9d (pg: 90, off:157)-->phy: 35997 (frm: 140) (prv: 139)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 36518 0x8ea6 (pg:142, off:166)-->phy: 14502 (frm:  56) (prv: 140)--> val:   35 == value:   35 --  +    HIT!
// log: 13361 0x3431 (pg: 52, off: 49)-->phy: 36145 (frm: 141) (prv: 140)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 19810 0x4d62 (pg: 77, off: 98)-->phy: 36450 (frm: 142) (prv: 141)--> val:   19 == value:   19 --  + ----> pg_fault

// log: 25955 0x6563 (pg:101, off: 99)-->phy: 36707 (frm: 143) (prv: 142)--> val:   88 == value:   88 --  + ----> pg_fault
// log: 62678 0xf4d6 (pg:244, off:214)-->phy:   470 (frm:   1) (prv: 143)--> val:   61 == value:   61 --  +    HIT!
// log: 26021 0x65a5 (pg:101, off:165)-->phy: 36773 (frm: 143) (prv: 143)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 29409 0x72e1 (pg:114, off:225)-->phy: 37089 (frm: 144) (prv: 143)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 38111 0x94df (pg:148, off:223)-->phy: 37343 (frm: 145) (prv: 144)--> val:   55 == value:   55 --  + ----> pg_fault

// log: 58573 0xe4cd (pg:228, off:205)-->phy: 15565 (frm:  60) (prv: 145)--> val:    0 == value:    0 --  +    HIT!
// log: 56840 0xde08 (pg:222, off:  8)-->phy: 37384 (frm: 146) (prv: 145)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 41306 0xa15a (pg:161, off: 90)-->phy: 24922 (frm:  97) (prv: 146)--> val:   40 == value:   40 --  +    HIT!
// log: 54426 0xd49a (pg:212, off:154)-->phy: 19354 (frm:  75) (prv: 146)--> val:   53 == value:   53 --  +    HIT!
// log:  3617 0x0e21 (pg: 14, off: 33)-->phy: 10017 (frm:  39) (prv: 146)--> val:    0 == value:    0 --  +    HIT!

// log: 50652 0xc5dc (pg:197, off:220)-->phy: 18652 (frm:  72) (prv: 146)--> val:    0 == value:    0 --  +    HIT!
// log: 41452 0xa1ec (pg:161, off:236)-->phy: 25068 (frm:  97) (prv: 146)--> val:    0 == value:    0 --  +    HIT!
// log: 20241 0x4f11 (pg: 79, off: 17)-->phy: 16657 (frm:  65) (prv: 146)--> val:    0 == value:    0 --  +    HIT!
// log: 31723 0x7beb (pg:123, off:235)-->phy: 12267 (frm:  47) (prv: 146)--> val:   -6 == value:   -6 --  +    HIT!
// log: 53747 0xd1f3 (pg:209, off:243)-->phy:  1011 (frm:   3) (prv: 146)--> val:  124 == value:  124 --  +    HIT!

// log: 28550 0x6f86 (pg:111, off:134)-->phy: 28038 (frm: 109) (prv: 146)--> val:   27 == value:   27 --  +    HIT!
// log: 23402 0x5b6a (pg: 91, off:106)-->phy: 20586 (frm:  80) (prv: 146)--> val:   22 == value:   22 --  +    HIT!
// log: 21205 0x52d5 (pg: 82, off:213)-->phy: 37845 (frm: 147) (prv: 146)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 56181 0xdb75 (pg:219, off:117)-->phy: 25205 (frm:  98) (prv: 147)--> val:    0 == value:    0 --  +    HIT!
// log: 57470 0xe07e (pg:224, off:126)-->phy: 38014 (frm: 148) (prv: 147)--> val:   56 == value:   56 --  + ----> pg_fault

// log: 39933 0x9bfd (pg:155, off:253)-->phy: 38397 (frm: 149) (prv: 148)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 34964 0x8894 (pg:136, off:148)-->phy: 26772 (frm: 104) (prv: 149)--> val:    0 == value:    0 --  +    HIT!
// log: 24781 0x60cd (pg: 96, off:205)-->phy: 38605 (frm: 150) (prv: 149)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 41747 0xa313 (pg:163, off: 19)-->phy: 38675 (frm: 151) (prv: 150)--> val:  -60 == value:  -60 --  + ----> pg_fault
// log: 62564 0xf464 (pg:244, off:100)-->phy:   356 (frm:   1) (prv: 151)--> val:    0 == value:    0 --  +    HIT!

// log: 58461 0xe45d (pg:228, off: 93)-->phy: 15453 (frm:  60) (prv: 151)--> val:    0 == value:    0 --  +    HIT!
// log: 20858 0x517a (pg: 81, off:122)-->phy: 39034 (frm: 152) (prv: 151)--> val:   20 == value:   20 --  + ----> pg_fault
// log: 49301 0xc095 (pg:192, off:149)-->phy:  8597 (frm:  33) (prv: 152)--> val:    0 == value:    0 --  +    HIT!
// log: 40572 0x9e7c (pg:158, off:124)-->phy: 39292 (frm: 153) (prv: 152)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 23840 0x5d20 (pg: 93, off: 32)-->phy: 39456 (frm: 154) (prv: 153)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 35278 0x89ce (pg:137, off:206)-->phy: 39886 (frm: 155) (prv: 154)--> val:   34 == value:   34 --  + ----> pg_fault
// log: 62905 0xf5b9 (pg:245, off:185)-->phy: 28601 (frm: 111) (prv: 155)--> val:    0 == value:    0 --  +    HIT!
// log: 56650 0xdd4a (pg:221, off: 74)-->phy: 13386 (frm:  52) (prv: 155)--> val:   55 == value:   55 --  +    HIT!
// log: 11149 0x2b8d (pg: 43, off:141)-->phy: 40077 (frm: 156) (prv: 155)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 38920 0x9808 (pg:152, off:  8)-->phy:  3848 (frm:  15) (prv: 156)--> val:    0 == value:    0 --  +    HIT!

// log: 23430 0x5b86 (pg: 91, off:134)-->phy: 20614 (frm:  80) (prv: 156)--> val:   22 == value:   22 --  +    HIT!
// log: 57592 0xe0f8 (pg:224, off:248)-->phy: 38136 (frm: 148) (prv: 156)--> val:    0 == value:    0 --  +    HIT!
// log:  3080 0x0c08 (pg: 12, off:  8)-->phy: 40200 (frm: 157) (prv: 156)--> val:    0 == value:    0 --  + ----> pg_fault
// log:  6677 0x1a15 (pg: 26, off: 21)-->phy:  6677 (frm:  26) (prv: 157)--> val:    0 == value:    0 --  +    HIT!
// log: 50704 0xc610 (pg:198, off: 16)-->phy: 26128 (frm: 102) (prv: 157)--> val:    0 == value:    0 --  +    HIT!

// log: 51883 0xcaab (pg:202, off:171)-->phy: 27307 (frm: 106) (prv: 157)--> val:  -86 == value:  -86 --  +    HIT!
// log: 62799 0xf54f (pg:245, off: 79)-->phy: 28495 (frm: 111) (prv: 157)--> val:   83 == value:   83 --  +    HIT!
// log: 20188 0x4edc (pg: 78, off:220)-->phy: 40668 (frm: 158) (prv: 157)--> val:    0 == value:    0 --  + ----> pg_fault
// log:  1245 0x04dd (pg:  4, off:221)-->phy: 40925 (frm: 159) (prv: 158)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 12220 0x2fbc (pg: 47, off:188)-->phy:  2748 (frm:  10) (prv: 159)--> val:    0 == value:    0 --  +    HIT!

// log: 17602 0x44c2 (pg: 68, off:194)-->phy: 41154 (frm: 160) (prv: 159)--> val:   17 == value:   17 --  + ----> pg_fault
// log: 28609 0x6fc1 (pg:111, off:193)-->phy: 28097 (frm: 109) (prv: 160)--> val:    0 == value:    0 --  +    HIT!
// log: 42694 0xa6c6 (pg:166, off:198)-->phy: 20934 (frm:  81) (prv: 160)--> val:   41 == value:   41 --  +    HIT!
// log: 29826 0x7482 (pg:116, off:130)-->phy: 41346 (frm: 161) (prv: 160)--> val:   29 == value:   29 --  + ----> pg_fault
// log: 13827 0x3603 (pg: 54, off:  3)-->phy: 31235 (frm: 122) (prv: 161)--> val: -128 == value: -128 --  +    HIT!

// log: 27336 0x6ac8 (pg:106, off:200)-->phy: 28872 (frm: 112) (prv: 161)--> val:    0 == value:    0 --  +    HIT!
// log: 53343 0xd05f (pg:208, off: 95)-->phy: 35423 (frm: 138) (prv: 161)--> val:   23 == value:   23 --  +    HIT!
// log: 11533 0x2d0d (pg: 45, off: 13)-->phy: 41485 (frm: 162) (prv: 161)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 41713 0xa2f1 (pg:162, off:241)-->phy: 41969 (frm: 163) (prv: 162)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 33890 0x8462 (pg:132, off: 98)-->phy: 42082 (frm: 164) (prv: 163)--> val:   33 == value:   33 --  + ----> pg_fault

// log:  4894 0x131e (pg: 19, off: 30)-->phy: 13598 (frm:  53) (prv: 164)--> val:    4 == value:    4 --  +    HIT!
// log: 57599 0xe0ff (pg:224, off:255)-->phy: 38143 (frm: 148) (prv: 164)--> val:   63 == value:   63 --  +    HIT!
// log:  3870 0x0f1e (pg: 15, off: 30)-->phy: 18974 (frm:  74) (prv: 164)--> val:    3 == value:    3 --  +    HIT!
// log: 58622 0xe4fe (pg:228, off:254)-->phy: 15614 (frm:  60) (prv: 164)--> val:   57 == value:   57 --  +    HIT!
// log: 29780 0x7454 (pg:116, off: 84)-->phy: 41300 (frm: 161) (prv: 164)--> val:    0 == value:    0 --  +    HIT!

// log: 62553 0xf459 (pg:244, off: 89)-->phy:   345 (frm:   1) (prv: 164)--> val:    0 == value:    0 --  +    HIT!
// log:  2303 0x08ff (pg:  8, off:255)-->phy: 35327 (frm: 137) (prv: 164)--> val:   63 == value:   63 --  +    HIT!
// log: 51915 0xcacb (pg:202, off:203)-->phy: 27339 (frm: 106) (prv: 164)--> val:  -78 == value:  -78 --  +    HIT!
// log:  6251 0x186b (pg: 24, off:107)-->phy:  7531 (frm:  29) (prv: 164)--> val:   26 == value:   26 --  +    HIT!
// log: 38107 0x94db (pg:148, off:219)-->phy: 37339 (frm: 145) (prv: 164)--> val:   54 == value:   54 --  +    HIT!

// log: 59325 0xe7bd (pg:231, off:189)-->phy: 10429 (frm:  40) (prv: 164)--> val:    0 == value:    0 --  +    HIT!
// log: 61295 0xef6f (pg:239, off:111)-->phy: 42351 (frm: 165) (prv: 164)--> val:  -37 == value:  -37 --  + ----> pg_fault
// log: 26699 0x684b (pg:104, off: 75)-->phy: 42571 (frm: 166) (prv: 165)--> val:   18 == value:   18 --  + ----> pg_fault
// log: 51188 0xc7f4 (pg:199, off:244)-->phy: 42996 (frm: 167) (prv: 166)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 59519 0xe87f (pg:232, off:127)-->phy: 25983 (frm: 101) (prv: 167)--> val:   31 == value:   31 --  +    HIT!

// log:  7345 0x1cb1 (pg: 28, off:177)-->phy: 43185 (frm: 168) (prv: 167)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 20325 0x4f65 (pg: 79, off:101)-->phy: 16741 (frm:  65) (prv: 168)--> val:    0 == value:    0 --  +    HIT!
// log: 39633 0x9ad1 (pg:154, off:209)-->phy: 43473 (frm: 169) (prv: 168)--> val:    0 == value:    0 --  + ----> pg_fault
// log:  1562 0x061a (pg:  6, off: 26)-->phy: 43546 (frm: 170) (prv: 169)--> val:    1 == value:    1 --  + ----> pg_fault
// log:  7580 0x1d9c (pg: 29, off:156)-->phy:  6300 (frm:  24) (prv: 170)--> val:    0 == value:    0 --  +    HIT!

// log:  8170 0x1fea (pg: 31, off:234)-->phy: 34026 (frm: 132) (prv: 170)--> val:    7 == value:    7 --  +    HIT!
// log: 62256 0xf330 (pg:243, off: 48)-->phy: 23856 (frm:  93) (prv: 170)--> val:    0 == value:    0 --  +    HIT!
// log: 35823 0x8bef (pg:139, off:239)-->phy: 44015 (frm: 171) (prv: 170)--> val:   -5 == value:   -5 --  + ----> pg_fault
// log: 27790 0x6c8e (pg:108, off:142)-->phy: 44174 (frm: 172) (prv: 171)--> val:   27 == value:   27 --  + ----> pg_fault
// log: 13191 0x3387 (pg: 51, off:135)-->phy: 44423 (frm: 173) (prv: 172)--> val:  -31 == value:  -31 --  + ----> pg_fault

// log:  9772 0x262c (pg: 38, off: 44)-->phy: 44588 (frm: 174) (prv: 173)--> val:    0 == value:    0 --  + ----> pg_fault
// log:  7477 0x1d35 (pg: 29, off: 53)-->phy:  6197 (frm:  24) (prv: 174)--> val:    0 == value:    0 --  +    HIT!
// log: 44455 0xada7 (pg:173, off:167)-->phy: 44967 (frm: 175) (prv: 174)--> val:  105 == value:  105 --  + ----> pg_fault
// log: 59546 0xe89a (pg:232, off:154)-->phy: 26010 (frm: 101) (prv: 175)--> val:   58 == value:   58 --  +    HIT!
// log: 49347 0xc0c3 (pg:192, off:195)-->phy:  8643 (frm:  33) (prv: 175)--> val:   48 == value:   48 --  +    HIT!

// log: 36539 0x8ebb (pg:142, off:187)-->phy: 14523 (frm:  56) (prv: 175)--> val:  -82 == value:  -82 --  +    HIT!
// log: 12453 0x30a5 (pg: 48, off:165)-->phy: 45221 (frm: 176) (prv: 175)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 49640 0xc1e8 (pg:193, off:232)-->phy: 45544 (frm: 177) (prv: 176)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 28290 0x6e82 (pg:110, off:130)-->phy: 34946 (frm: 136) (prv: 177)--> val:   27 == value:   27 --  +    HIT!
// log: 44817 0xaf11 (pg:175, off: 17)-->phy: 13073 (frm:  51) (prv: 177)--> val:    0 == value:    0 --  +    HIT!

// log:  8565 0x2175 (pg: 33, off:117)-->phy: 20085 (frm:  78) (prv: 177)--> val:    0 == value:    0 --  +    HIT!
// log: 16399 0x400f (pg: 64, off: 15)-->phy: 45583 (frm: 178) (prv: 177)--> val:    3 == value:    3 --  + ----> pg_fault
// log: 41934 0xa3ce (pg:163, off:206)-->phy: 38862 (frm: 151) (prv: 178)--> val:   40 == value:   40 --  +    HIT!
// log: 45457 0xb191 (pg:177, off:145)-->phy: 45969 (frm: 179) (prv: 178)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 33856 0x8440 (pg:132, off: 64)-->phy: 42048 (frm: 164) (prv: 179)--> val:    0 == value:    0 --  +    HIT!

// log: 19498 0x4c2a (pg: 76, off: 42)-->phy: 22570 (frm:  88) (prv: 179)--> val:   19 == value:   19 --  +    HIT!
// log: 17661 0x44fd (pg: 68, off:253)-->phy: 41213 (frm: 160) (prv: 179)--> val:    0 == value:    0 --  +    HIT!
// log: 63829 0xf955 (pg:249, off: 85)-->phy: 21845 (frm:  85) (prv: 179)--> val:    0 == value:    0 --  +    HIT!
// log: 42034 0xa432 (pg:164, off: 50)-->phy: 46130 (frm: 180) (prv: 179)--> val:   41 == value:   41 --  + ----> pg_fault
// log: 28928 0x7100 (pg:113, off:  0)-->phy: 16384 (frm:  64) (prv: 180)--> val:    0 == value:    0 --  +    HIT!

// log: 30711 0x77f7 (pg:119, off:247)-->phy: 16375 (frm:  63) (prv: 180)--> val:   -3 == value:   -3 --  +    HIT!
// log:  8800 0x2260 (pg: 34, off: 96)-->phy: 46432 (frm: 181) (prv: 180)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 52335 0xcc6f (pg:204, off:111)-->phy: 46703 (frm: 182) (prv: 181)--> val:   27 == value:   27 --  + ----> pg_fault
// log: 38775 0x9777 (pg:151, off:119)-->phy: 46967 (frm: 183) (prv: 182)--> val:  -35 == value:  -35 --  + ----> pg_fault
// log: 52704 0xcde0 (pg:205, off:224)-->phy: 30176 (frm: 117) (prv: 183)--> val:    0 == value:    0 --  +    HIT!

// log: 24380 0x5f3c (pg: 95, off: 60)-->phy:  1596 (frm:   6) (prv: 183)--> val:    0 == value:    0 --  +    HIT!
// log: 19602 0x4c92 (pg: 76, off:146)-->phy: 22674 (frm:  88) (prv: 183)--> val:   19 == value:   19 --  +    HIT!
// log: 57998 0xe28e (pg:226, off:142)-->phy:  3214 (frm:  12) (prv: 183)--> val:   56 == value:   56 --  +    HIT!
// log:  2919 0x0b67 (pg: 11, off:103)-->phy: 11623 (frm:  45) (prv: 183)--> val:  -39 == value:  -39 --  +    HIT!
// log:  8362 0x20aa (pg: 32, off:170)-->phy: 47274 (frm: 184) (prv: 183)--> val:    8 == value:    8 --  + ----> pg_fault

// log: 17884 0x45dc (pg: 69, off:220)-->phy:  9948 (frm:  38) (prv: 184)--> val:    0 == value:    0 --  +    HIT!
// log: 45737 0xb2a9 (pg:178, off:169)-->phy:  7849 (frm:  30) (prv: 184)--> val:    0 == value:    0 --  +    HIT!
// log: 47894 0xbb16 (pg:187, off: 22)-->phy: 25622 (frm: 100) (prv: 184)--> val:   46 == value:   46 --  +    HIT!
// log: 59667 0xe913 (pg:233, off: 19)-->phy: 47379 (frm: 185) (prv: 184)--> val:   68 == value:   68 --  + ----> pg_fault
// log: 10385 0x2891 (pg: 40, off:145)-->phy: 14737 (frm:  57) (prv: 185)--> val:    0 == value:    0 --  +    HIT!

// log: 52782 0xce2e (pg:206, off: 46)-->phy: 47662 (frm: 186) (prv: 185)--> val:   51 == value:   51 --  + ----> pg_fault
// log: 64416 0xfba0 (pg:251, off:160)-->phy:  5024 (frm:  19) (prv: 186)--> val:    0 == value:    0 --  +    HIT!
// log: 40946 0x9ff2 (pg:159, off:242)-->phy:  8434 (frm:  32) (prv: 186)--> val:   39 == value:   39 --  +    HIT!
// log: 16778 0x418a (pg: 65, off:138)-->phy: 48010 (frm: 187) (prv: 186)--> val:   16 == value:   16 --  + ----> pg_fault
// log: 27159 0x6a17 (pg:106, off: 23)-->phy: 28695 (frm: 112) (prv: 187)--> val: -123 == value: -123 --  +    HIT!

// log: 24324 0x5f04 (pg: 95, off:  4)-->phy:  1540 (frm:   6) (prv: 187)--> val:    0 == value:    0 --  +    HIT!
// log: 32450 0x7ec2 (pg:126, off:194)-->phy:  7106 (frm:  27) (prv: 187)--> val:   31 == value:   31 --  +    HIT!
// log:  9108 0x2394 (pg: 35, off:148)-->phy: 48276 (frm: 188) (prv: 187)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 65305 0xff19 (pg:255, off: 25)-->phy: 30489 (frm: 119) (prv: 188)--> val:    0 == value:    0 --  +    HIT!
// log: 19575 0x4c77 (pg: 76, off:119)-->phy: 22647 (frm:  88) (prv: 188)--> val:   29 == value:   29 --  +    HIT!

// log: 11117 0x2b6d (pg: 43, off:109)-->phy: 40045 (frm: 156) (prv: 188)--> val:    0 == value:    0 --  +    HIT!
// log: 65170 0xfe92 (pg:254, off:146)-->phy: 48530 (frm: 189) (prv: 188)--> val:   63 == value:   63 --  + ----> pg_fault
// log: 58013 0xe29d (pg:226, off:157)-->phy:  3229 (frm:  12) (prv: 189)--> val:    0 == value:    0 --  +    HIT!
// log: 61676 0xf0ec (pg:240, off:236)-->phy: 23276 (frm:  90) (prv: 189)--> val:    0 == value:    0 --  +    HIT!
// log: 63510 0xf816 (pg:248, off: 22)-->phy: 28182 (frm: 110) (prv: 189)--> val:   62 == value:   62 --  +    HIT!

// log: 17458 0x4432 (pg: 68, off: 50)-->phy: 41010 (frm: 160) (prv: 189)--> val:   17 == value:   17 --  +    HIT!
// log: 54675 0xd593 (pg:213, off:147)-->phy: 48787 (frm: 190) (prv: 189)--> val:  100 == value:  100 --  + ----> pg_fault
// log:  1713 0x06b1 (pg:  6, off:177)-->phy: 43697 (frm: 170) (prv: 190)--> val:    0 == value:    0 --  +    HIT!
// log: 55105 0xd741 (pg:215, off: 65)-->phy:  5185 (frm:  20) (prv: 190)--> val:    0 == value:    0 --  +    HIT!
// log: 65321 0xff29 (pg:255, off: 41)-->phy: 30505 (frm: 119) (prv: 190)--> val:    0 == value:    0 --  +    HIT!

// log: 45278 0xb0de (pg:176, off:222)-->phy: 49118 (frm: 191) (prv: 190)--> val:   44 == value:   44 --  + ----> pg_fault
// log: 26256 0x6690 (pg:102, off:144)-->phy: 49296 (frm: 192) (prv: 191)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 64198 0xfac6 (pg:250, off:198)-->phy:  4550 (frm:  17) (prv: 192)--> val:   62 == value:   62 --  +    HIT!
// log: 29441 0x7301 (pg:115, off:  1)-->phy: 49409 (frm: 193) (prv: 192)--> val:    0 == value:    0 --  + ----> pg_fault
// log:  1928 0x0788 (pg:  7, off:136)-->phy: 33160 (frm: 129) (prv: 193)--> val:    0 == value:    0 --  +    HIT!

// log: 39425 0x9a01 (pg:154, off:  1)-->phy: 43265 (frm: 169) (prv: 193)--> val:    0 == value:    0 --  +    HIT!
// log: 32000 0x7d00 (pg:125, off:  0)-->phy: 49664 (frm: 194) (prv: 193)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 28549 0x6f85 (pg:111, off:133)-->phy: 28037 (frm: 109) (prv: 194)--> val:    0 == value:    0 --  +    HIT!
// log: 46295 0xb4d7 (pg:180, off:215)-->phy: 50135 (frm: 195) (prv: 194)--> val:   53 == value:   53 --  + ----> pg_fault
// log: 22772 0x58f4 (pg: 88, off:244)-->phy:  3060 (frm:  11) (prv: 195)--> val:    0 == value:    0 --  +    HIT!

// log: 58228 0xe374 (pg:227, off:116)-->phy: 34164 (frm: 133) (prv: 195)--> val:    0 == value:    0 --  +    HIT!
// log: 63525 0xf825 (pg:248, off: 37)-->phy: 28197 (frm: 110) (prv: 195)--> val:    0 == value:    0 --  +    HIT!
// log: 32602 0x7f5a (pg:127, off: 90)-->phy:  9562 (frm:  37) (prv: 195)--> val:   31 == value:   31 --  +    HIT!
// log: 46195 0xb473 (pg:180, off:115)-->phy: 50035 (frm: 195) (prv: 195)--> val:   28 == value:   28 --  + ----> pg_fault
// log: 55849 0xda29 (pg:218, off: 41)-->phy: 34601 (frm: 135) (prv: 195)--> val:    0 == value:    0 --  +    HIT!

// log: 46454 0xb576 (pg:181, off:118)-->phy: 50294 (frm: 196) (prv: 195)--> val:   45 == value:   45 --  + ----> pg_fault
// log:  7487 0x1d3f (pg: 29, off: 63)-->phy:  6207 (frm:  24) (prv: 196)--> val:   79 == value:   79 --  +    HIT!
// log: 33879 0x8457 (pg:132, off: 87)-->phy: 42071 (frm: 164) (prv: 196)--> val:   21 == value:   21 --  +    HIT!
// log: 42004 0xa414 (pg:164, off: 20)-->phy: 46100 (frm: 180) (prv: 196)--> val:    0 == value:    0 --  +    HIT!
// log:  8599 0x2197 (pg: 33, off:151)-->phy: 20119 (frm:  78) (prv: 196)--> val:  101 == value:  101 --  +    HIT!

// log: 18641 0x48d1 (pg: 72, off:209)-->phy:  5585 (frm:  21) (prv: 196)--> val:    0 == value:    0 --  +    HIT!
// log: 49015 0xbf77 (pg:191, off:119)-->phy: 50551 (frm: 197) (prv: 196)--> val:  -35 == value:  -35 --  + ----> pg_fault
// log: 26830 0x68ce (pg:104, off:206)-->phy: 42702 (frm: 166) (prv: 197)--> val:   26 == value:   26 --  +    HIT!
// log: 34754 0x87c2 (pg:135, off:194)-->phy: 18114 (frm:  70) (prv: 197)--> val:   33 == value:   33 --  +    HIT!
// log: 14668 0x394c (pg: 57, off: 76)-->phy: 33356 (frm: 130) (prv: 197)--> val:    0 == value:    0 --  +    HIT!

// log: 38362 0x95da (pg:149, off:218)-->phy: 20442 (frm:  79) (prv: 197)--> val:   37 == value:   37 --  +    HIT!
// log: 38791 0x9787 (pg:151, off:135)-->phy: 46983 (frm: 183) (prv: 197)--> val:  -31 == value:  -31 --  +    HIT!
// log:  4171 0x104b (pg: 16, off: 75)-->phy: 50763 (frm: 198) (prv: 197)--> val:   18 == value:   18 --  + ----> pg_fault
// log: 45975 0xb397 (pg:179, off:151)-->phy: 51095 (frm: 199) (prv: 198)--> val:  -27 == value:  -27 --  + ----> pg_fault
// log: 14623 0x391f (pg: 57, off: 31)-->phy: 33311 (frm: 130) (prv: 199)--> val:   71 == value:   71 --  +    HIT!

// log: 62393 0xf3b9 (pg:243, off:185)-->phy: 23993 (frm:  93) (prv: 199)--> val:    0 == value:    0 --  +    HIT!
// log: 64658 0xfc92 (pg:252, off:146)-->phy:  6546 (frm:  25) (prv: 199)--> val:   63 == value:   63 --  +    HIT!
// log: 10963 0x2ad3 (pg: 42, off:211)-->phy: 12499 (frm:  48) (prv: 199)--> val:  -76 == value:  -76 --  +    HIT!
// log:  9058 0x2362 (pg: 35, off: 98)-->phy: 48226 (frm: 188) (prv: 199)--> val:    8 == value:    8 --  +    HIT!
// log: 51031 0xc757 (pg:199, off: 87)-->phy: 42839 (frm: 167) (prv: 199)--> val:  -43 == value:  -43 --  +    HIT!

// log: 32425 0x7ea9 (pg:126, off:169)-->phy:  7081 (frm:  27) (prv: 199)--> val:    0 == value:    0 --  +    HIT!
// log: 45483 0xb1ab (pg:177, off:171)-->phy: 45995 (frm: 179) (prv: 199)--> val:  106 == value:  106 --  +    HIT!
// log: 44611 0xae43 (pg:174, off: 67)-->phy: 11075 (frm:  43) (prv: 199)--> val: -112 == value: -112 --  +    HIT!
// log: 63664 0xf8b0 (pg:248, off:176)-->phy: 28336 (frm: 110) (prv: 199)--> val:    0 == value:    0 --  +    HIT!
// log: 54920 0xd688 (pg:214, off:136)-->phy:  3720 (frm:  14) (prv: 199)--> val:    0 == value:    0 --  +    HIT!

// log:  7663 0x1def (pg: 29, off:239)-->phy:  6383 (frm:  24) (prv: 199)--> val:  123 == value:  123 --  +    HIT!
// log: 56480 0xdca0 (pg:220, off:160)-->phy: 51360 (frm: 200) (prv: 199)--> val:    0 == value:    0 --  + ----> pg_fault
// log:  1489 0x05d1 (pg:  5, off:209)-->phy: 51665 (frm: 201) (prv: 200)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 28438 0x6f16 (pg:111, off: 22)-->phy: 27926 (frm: 109) (prv: 201)--> val:   27 == value:   27 --  +    HIT!
// log: 65449 0xffa9 (pg:255, off:169)-->phy: 30633 (frm: 119) (prv: 201)--> val:    0 == value:    0 --  +    HIT!

// log: 12441 0x3099 (pg: 48, off:153)-->phy: 45209 (frm: 176) (prv: 201)--> val:    0 == value:    0 --  +    HIT!
// log: 58530 0xe4a2 (pg:228, off:162)-->phy: 15522 (frm:  60) (prv: 201)--> val:   57 == value:   57 --  +    HIT!
// log: 63570 0xf852 (pg:248, off: 82)-->phy: 28242 (frm: 110) (prv: 201)--> val:   62 == value:   62 --  +    HIT!
// log: 26251 0x668b (pg:102, off:139)-->phy: 49291 (frm: 192) (prv: 201)--> val:  -94 == value:  -94 --  +    HIT!
// log: 15972 0x3e64 (pg: 62, off:100)-->phy: 21092 (frm:  82) (prv: 201)--> val:    0 == value:    0 --  +    HIT!

// log: 35826 0x8bf2 (pg:139, off:242)-->phy: 44018 (frm: 171) (prv: 201)--> val:   34 == value:   34 --  +    HIT!
// log:  5491 0x1573 (pg: 21, off:115)-->phy: 21619 (frm:  84) (prv: 201)--> val:   92 == value:   92 --  +    HIT!
// log: 54253 0xd3ed (pg:211, off:237)-->phy: 51949 (frm: 202) (prv: 201)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 49655 0xc1f7 (pg:193, off:247)-->phy: 45559 (frm: 177) (prv: 202)--> val:  125 == value:  125 --  +    HIT!
// log:  5868 0x16ec (pg: 22, off:236)-->phy: 52204 (frm: 203) (prv: 202)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 20163 0x4ec3 (pg: 78, off:195)-->phy: 40643 (frm: 158) (prv: 203)--> val:  -80 == value:  -80 --  +    HIT!
// log: 51079 0xc787 (pg:199, off:135)-->phy: 42887 (frm: 167) (prv: 203)--> val:  -31 == value:  -31 --  +    HIT!
// log: 21398 0x5396 (pg: 83, off:150)-->phy:  9110 (frm:  35) (prv: 203)--> val:   20 == value:   20 --  +    HIT!
// log: 32756 0x7ff4 (pg:127, off:244)-->phy:  9716 (frm:  37) (prv: 203)--> val:    0 == value:    0 --  +    HIT!
// log: 64196 0xfac4 (pg:250, off:196)-->phy:  4548 (frm:  17) (prv: 203)--> val:    0 == value:    0 --  +    HIT!

// log: 43218 0xa8d2 (pg:168, off:210)-->phy: 17618 (frm:  68) (prv: 203)--> val:   42 == value:   42 --  +    HIT!
// log: 21583 0x544f (pg: 84, off: 79)-->phy: 52303 (frm: 204) (prv: 203)--> val:   19 == value:   19 --  + ----> pg_fault
// log: 25086 0x61fe (pg: 97, off:254)-->phy: 52734 (frm: 205) (prv: 204)--> val:   24 == value:   24 --  + ----> pg_fault
// log: 45515 0xb1cb (pg:177, off:203)-->phy: 46027 (frm: 179) (prv: 205)--> val:  114 == value:  114 --  +    HIT!
// log: 12893 0x325d (pg: 50, off: 93)-->phy: 30301 (frm: 118) (prv: 205)--> val:    0 == value:    0 --  +    HIT!

// log: 22914 0x5982 (pg: 89, off:130)-->phy: 52866 (frm: 206) (prv: 205)--> val:   22 == value:   22 --  + ----> pg_fault
// log: 58969 0xe659 (pg:230, off: 89)-->phy: 14937 (frm:  58) (prv: 206)--> val:    0 == value:    0 --  +    HIT!
// log: 20094 0x4e7e (pg: 78, off:126)-->phy: 40574 (frm: 158) (prv: 206)--> val:   19 == value:   19 --  +    HIT!
// log: 13730 0x35a2 (pg: 53, off:162)-->phy: 53154 (frm: 207) (prv: 206)--> val:   13 == value:   13 --  + ----> pg_fault
// log: 44059 0xac1b (pg:172, off: 27)-->phy: 53275 (frm: 208) (prv: 207)--> val:    6 == value:    6 --  + ----> pg_fault

// log: 28931 0x7103 (pg:113, off:  3)-->phy: 16387 (frm:  64) (prv: 208)--> val:   64 == value:   64 --  +    HIT!
// log: 13533 0x34dd (pg: 52, off:221)-->phy: 36317 (frm: 141) (prv: 208)--> val:    0 == value:    0 --  +    HIT!
// log: 33134 0x816e (pg:129, off:110)-->phy: 53614 (frm: 209) (prv: 208)--> val:   32 == value:   32 --  + ----> pg_fault
// log: 28483 0x6f43 (pg:111, off: 67)-->phy: 27971 (frm: 109) (prv: 209)--> val:  -48 == value:  -48 --  +    HIT!
// log:  1220 0x04c4 (pg:  4, off:196)-->phy: 40900 (frm: 159) (prv: 209)--> val:    0 == value:    0 --  +    HIT!

// log: 38174 0x951e (pg:149, off: 30)-->phy: 20254 (frm:  79) (prv: 209)--> val:   37 == value:   37 --  +    HIT!
// log: 53502 0xd0fe (pg:208, off:254)-->phy: 35582 (frm: 138) (prv: 209)--> val:   52 == value:   52 --  +    HIT!
// log: 43328 0xa940 (pg:169, off: 64)-->phy: 29504 (frm: 115) (prv: 209)--> val:    0 == value:    0 --  +    HIT!
// log:  4970 0x136a (pg: 19, off:106)-->phy: 13674 (frm:  53) (prv: 209)--> val:    4 == value:    4 --  +    HIT!
// log:  8090 0x1f9a (pg: 31, off:154)-->phy: 33946 (frm: 132) (prv: 209)--> val:    7 == value:    7 --  +    HIT!

// log:  2661 0x0a65 (pg: 10, off:101)-->phy: 53861 (frm: 210) (prv: 209)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 53903 0xd28f (pg:210, off:143)-->phy: 54159 (frm: 211) (prv: 210)--> val:  -93 == value:  -93 --  + ----> pg_fault
// log: 11025 0x2b11 (pg: 43, off: 17)-->phy: 39953 (frm: 156) (prv: 211)--> val:    0 == value:    0 --  +    HIT!
// log: 26627 0x6803 (pg:104, off:  3)-->phy: 42499 (frm: 166) (prv: 211)--> val:    0 == value:    0 --  +    HIT!
// log: 18117 0x46c5 (pg: 70, off:197)-->phy: 18885 (frm:  73) (prv: 211)--> val:    0 == value:    0 --  +    HIT!

// log: 14505 0x38a9 (pg: 56, off:169)-->phy:  5801 (frm:  22) (prv: 211)--> val:    0 == value:    0 --  +    HIT!
// log: 61528 0xf058 (pg:240, off: 88)-->phy: 23128 (frm:  90) (prv: 211)--> val:    0 == value:    0 --  +    HIT!
// log: 20423 0x4fc7 (pg: 79, off:199)-->phy: 16839 (frm:  65) (prv: 211)--> val:  -15 == value:  -15 --  +    HIT!
// log: 26962 0x6952 (pg:105, off: 82)-->phy: 26962 (frm: 105) (prv: 211)--> val:   26 == value:   26 --  +    HIT!
// log: 36392 0x8e28 (pg:142, off: 40)-->phy: 14376 (frm:  56) (prv: 211)--> val:    0 == value:    0 --  +    HIT!

// log: 11365 0x2c65 (pg: 44, off:101)-->phy: 54373 (frm: 212) (prv: 211)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 50882 0xc6c2 (pg:198, off:194)-->phy: 26306 (frm: 102) (prv: 212)--> val:   49 == value:   49 --  +    HIT!
// log: 41668 0xa2c4 (pg:162, off:196)-->phy: 41924 (frm: 163) (prv: 212)--> val:    0 == value:    0 --  +    HIT!
// log: 30497 0x7721 (pg:119, off: 33)-->phy: 16161 (frm:  63) (prv: 212)--> val:    0 == value:    0 --  +    HIT!
// log: 36216 0x8d78 (pg:141, off:120)-->phy: 54648 (frm: 213) (prv: 212)--> val:    0 == value:    0 --  + ----> pg_fault

// log:  5619 0x15f3 (pg: 21, off:243)-->phy: 21747 (frm:  84) (prv: 213)--> val:  124 == value:  124 --  +    HIT!
// log: 36983 0x9077 (pg:144, off:119)-->phy: 18295 (frm:  71) (prv: 213)--> val:   29 == value:   29 --  +    HIT!
// log: 59557 0xe8a5 (pg:232, off:165)-->phy: 26021 (frm: 101) (prv: 213)--> val:    0 == value:    0 --  +    HIT!
// log: 36663 0x8f37 (pg:143, off: 55)-->phy: 35639 (frm: 139) (prv: 213)--> val:  -51 == value:  -51 --  +    HIT!
// log: 36436 0x8e54 (pg:142, off: 84)-->phy: 14420 (frm:  56) (prv: 213)--> val:    0 == value:    0 --  +    HIT!

// log: 37057 0x90c1 (pg:144, off:193)-->phy: 18369 (frm:  71) (prv: 213)--> val:    0 == value:    0 --  +    HIT!
// log: 23585 0x5c21 (pg: 92, off: 33)-->phy: 54817 (frm: 214) (prv: 213)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 58791 0xe5a7 (pg:229, off:167)-->phy: 55207 (frm: 215) (prv: 214)--> val:  105 == value:  105 --  + ----> pg_fault
// log: 46666 0xb64a (pg:182, off: 74)-->phy: 55370 (frm: 216) (prv: 215)--> val:   45 == value:   45 --  + ----> pg_fault
// log: 64475 0xfbdb (pg:251, off:219)-->phy:  5083 (frm:  19) (prv: 216)--> val:  -10 == value:  -10 --  +    HIT!

// log: 21615 0x546f (pg: 84, off:111)-->phy: 52335 (frm: 204) (prv: 216)--> val:   27 == value:   27 --  +    HIT!
// log: 41090 0xa082 (pg:160, off:130)-->phy:  8834 (frm:  34) (prv: 216)--> val:   40 == value:   40 --  +    HIT!
// log:  1771 0x06eb (pg:  6, off:235)-->phy: 43755 (frm: 170) (prv: 216)--> val:  -70 == value:  -70 --  +    HIT!
// log: 47513 0xb999 (pg:185, off:153)-->phy: 32153 (frm: 125) (prv: 216)--> val:    0 == value:    0 --  +    HIT!
// log: 39338 0x99aa (pg:153, off:170)-->phy: 55722 (frm: 217) (prv: 216)--> val:   38 == value:   38 --  + ----> pg_fault

// log:  1390 0x056e (pg:  5, off:110)-->phy: 51566 (frm: 201) (prv: 217)--> val:    1 == value:    1 --  +    HIT!
// log: 38772 0x9774 (pg:151, off:116)-->phy: 46964 (frm: 183) (prv: 217)--> val:    0 == value:    0 --  +    HIT!
// log: 58149 0xe325 (pg:227, off: 37)-->phy: 34085 (frm: 133) (prv: 217)--> val:    0 == value:    0 --  +    HIT!
// log:  7196 0x1c1c (pg: 28, off: 28)-->phy: 43036 (frm: 168) (prv: 217)--> val:    0 == value:    0 --  +    HIT!
// log:  9123 0x23a3 (pg: 35, off:163)-->phy: 48291 (frm: 188) (prv: 217)--> val:  -24 == value:  -24 --  +    HIT!

// log:  7491 0x1d43 (pg: 29, off: 67)-->phy:  6211 (frm:  24) (prv: 217)--> val:   80 == value:   80 --  +    HIT!
// log: 62616 0xf498 (pg:244, off:152)-->phy:   408 (frm:   1) (prv: 217)--> val:    0 == value:    0 --  +    HIT!
// log: 15436 0x3c4c (pg: 60, off: 76)-->phy: 31820 (frm: 124) (prv: 217)--> val:    0 == value:    0 --  +    HIT!
// log: 17491 0x4453 (pg: 68, off: 83)-->phy: 41043 (frm: 160) (prv: 217)--> val:   20 == value:   20 --  +    HIT!
// log: 53656 0xd198 (pg:209, off:152)-->phy:   920 (frm:   3) (prv: 217)--> val:    0 == value:    0 --  +    HIT!

// log: 26449 0x6751 (pg:103, off: 81)-->phy: 24401 (frm:  95) (prv: 217)--> val:    0 == value:    0 --  +    HIT!
// log: 34935 0x8877 (pg:136, off:119)-->phy: 26743 (frm: 104) (prv: 217)--> val:   29 == value:   29 --  +    HIT!
// log: 19864 0x4d98 (pg: 77, off:152)-->phy: 36504 (frm: 142) (prv: 217)--> val:    0 == value:    0 --  +    HIT!
// log: 51388 0xc8bc (pg:200, off:188)-->phy: 32700 (frm: 127) (prv: 217)--> val:    0 == value:    0 --  +    HIT!
// log: 15155 0x3b33 (pg: 59, off: 51)-->phy: 32307 (frm: 126) (prv: 217)--> val:  -52 == value:  -52 --  +    HIT!

// log: 64775 0xfd07 (pg:253, off:  7)-->phy:  2055 (frm:   8) (prv: 217)--> val:   65 == value:   65 --  +    HIT!
// log: 47969 0xbb61 (pg:187, off: 97)-->phy: 25697 (frm: 100) (prv: 217)--> val:    0 == value:    0 --  +    HIT!
// log: 16315 0x3fbb (pg: 63, off:187)-->phy: 55995 (frm: 218) (prv: 217)--> val:  -18 == value:  -18 --  + ----> pg_fault
// log:  1342 0x053e (pg:  5, off: 62)-->phy: 51518 (frm: 201) (prv: 218)--> val:    1 == value:    1 --  +    HIT!
// log: 51185 0xc7f1 (pg:199, off:241)-->phy: 42993 (frm: 167) (prv: 218)--> val:    0 == value:    0 --  +    HIT!

// log:  6043 0x179b (pg: 23, off:155)-->phy:  9371 (frm:  36) (prv: 218)--> val:  -26 == value:  -26 --  +    HIT!
// log: 21398 0x5396 (pg: 83, off:150)-->phy:  9110 (frm:  35) (prv: 218)--> val:   20 == value:   20 --  +    HIT!
// log:  3273 0x0cc9 (pg: 12, off:201)-->phy: 40393 (frm: 157) (prv: 218)--> val:    0 == value:    0 --  +    HIT!
// log:  9370 0x249a (pg: 36, off:154)-->phy: 22938 (frm:  89) (prv: 218)--> val:    9 == value:    9 --  +    HIT!
// log: 35463 0x8a87 (pg:138, off:135)-->phy: 56199 (frm: 219) (prv: 218)--> val:  -95 == value:  -95 --  + ----> pg_fault

// log: 28205 0x6e2d (pg:110, off: 45)-->phy: 34861 (frm: 136) (prv: 219)--> val:    0 == value:    0 --  +    HIT!
// log:  2351 0x092f (pg:  9, off: 47)-->phy:  4655 (frm:  18) (prv: 219)--> val:   75 == value:   75 --  +    HIT!
// log: 28999 0x7147 (pg:113, off: 71)-->phy: 16455 (frm:  64) (prv: 219)--> val:   81 == value:   81 --  +    HIT!
// log: 47699 0xba53 (pg:186, off: 83)-->phy: 56403 (frm: 220) (prv: 219)--> val: -108 == value: -108 --  + ----> pg_fault
// log: 46870 0xb716 (pg:183, off: 22)-->phy: 19734 (frm:  77) (prv: 220)--> val:   45 == value:   45 --  +    HIT!

// log: 22311 0x5727 (pg: 87, off: 39)-->phy: 11303 (frm:  44) (prv: 220)--> val:  -55 == value:  -55 --  +    HIT!
// log: 22124 0x566c (pg: 86, off:108)-->phy: 56684 (frm: 221) (prv: 220)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 22427 0x579b (pg: 87, off:155)-->phy: 11419 (frm:  44) (prv: 221)--> val:  -26 == value:  -26 --  +    HIT!
// log: 49344 0xc0c0 (pg:192, off:192)-->phy:  8640 (frm:  33) (prv: 221)--> val:    0 == value:    0 --  +    HIT!
// log: 23224 0x5ab8 (pg: 90, off:184)-->phy: 36024 (frm: 140) (prv: 221)--> val:    0 == value:    0 --  +    HIT!

// log:  5514 0x158a (pg: 21, off:138)-->phy: 21642 (frm:  84) (prv: 221)--> val:    5 == value:    5 --  +    HIT!
// log: 20504 0x5018 (pg: 80, off: 24)-->phy: 56856 (frm: 222) (prv: 221)--> val:    0 == value:    0 --  + ----> pg_fault
// log:   376 0x0178 (pg:  1, off:120)-->phy: 57208 (frm: 223) (prv: 222)--> val:    0 == value:    0 --  + ----> pg_fault
// log:  2014 0x07de (pg:  7, off:222)-->phy: 33246 (frm: 129) (prv: 223)--> val:    1 == value:    1 --  +    HIT!
// log: 38700 0x972c (pg:151, off: 44)-->phy: 46892 (frm: 183) (prv: 223)--> val:    0 == value:    0 --  +    HIT!

// log: 13098 0x332a (pg: 51, off: 42)-->phy: 44330 (frm: 173) (prv: 223)--> val:   12 == value:   12 --  +    HIT!
// log: 62435 0xf3e3 (pg:243, off:227)-->phy: 24035 (frm:  93) (prv: 223)--> val:   -8 == value:   -8 --  +    HIT!
// log: 48046 0xbbae (pg:187, off:174)-->phy: 25774 (frm: 100) (prv: 223)--> val:   46 == value:   46 --  +    HIT!
// log: 63464 0xf7e8 (pg:247, off:232)-->phy: 17128 (frm:  66) (prv: 223)--> val:    0 == value:    0 --  +    HIT!
// log: 12798 0x31fe (pg: 49, off:254)-->phy: 31230 (frm: 121) (prv: 223)--> val:   12 == value:   12 --  +    HIT!

// log: 51178 0xc7ea (pg:199, off:234)-->phy: 42986 (frm: 167) (prv: 223)--> val:   49 == value:   49 --  +    HIT!
// log:  8627 0x21b3 (pg: 33, off:179)-->phy: 20147 (frm:  78) (prv: 223)--> val:  108 == value:  108 --  +    HIT!
// log: 27083 0x69cb (pg:105, off:203)-->phy: 27083 (frm: 105) (prv: 223)--> val:  114 == value:  114 --  +    HIT!
// log: 47198 0xb85e (pg:184, off: 94)-->phy: 29790 (frm: 116) (prv: 223)--> val:   46 == value:   46 --  +    HIT!
// log: 44021 0xabf5 (pg:171, off:245)-->phy: 57589 (frm: 224) (prv: 223)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 32792 0x8018 (pg:128, off: 24)-->phy:  4120 (frm:  16) (prv: 224)--> val:    0 == value:    0 --  +    HIT!
// log: 43996 0xabdc (pg:171, off:220)-->phy: 57564 (frm: 224) (prv: 224)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 41126 0xa0a6 (pg:160, off:166)-->phy:  8870 (frm:  34) (prv: 224)--> val:   40 == value:   40 --  +    HIT!
// log: 64244 0xfaf4 (pg:250, off:244)-->phy:  4596 (frm:  17) (prv: 224)--> val:    0 == value:    0 --  +    HIT!
// log: 37047 0x90b7 (pg:144, off:183)-->phy: 18359 (frm:  71) (prv: 224)--> val:   45 == value:   45 --  +    HIT!

// log: 60281 0xeb79 (pg:235, off:121)-->phy: 57721 (frm: 225) (prv: 224)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 52904 0xcea8 (pg:206, off:168)-->phy: 47784 (frm: 186) (prv: 225)--> val:    0 == value:    0 --  +    HIT!
// log:  7768 0x1e58 (pg: 30, off: 88)-->phy: 57944 (frm: 226) (prv: 225)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 55359 0xd83f (pg:216, off: 63)-->phy: 58175 (frm: 227) (prv: 226)--> val:   15 == value:   15 --  + ----> pg_fault
// log:  3230 0x0c9e (pg: 12, off:158)-->phy: 40350 (frm: 157) (prv: 227)--> val:    3 == value:    3 --  +    HIT!

// log: 44813 0xaf0d (pg:175, off: 13)-->phy: 13069 (frm:  51) (prv: 227)--> val:    0 == value:    0 --  +    HIT!
// log:  4116 0x1014 (pg: 16, off: 20)-->phy: 50708 (frm: 198) (prv: 227)--> val:    0 == value:    0 --  +    HIT!
// log: 65222 0xfec6 (pg:254, off:198)-->phy: 48582 (frm: 189) (prv: 227)--> val:   63 == value:   63 --  +    HIT!
// log: 28083 0x6db3 (pg:109, off:179)-->phy:  3507 (frm:  13) (prv: 227)--> val:  108 == value:  108 --  +    HIT!
// log: 60660 0xecf4 (pg:236, off:244)-->phy:  7412 (frm:  28) (prv: 227)--> val:    0 == value:    0 --  +    HIT!

// log:    39 0x0027 (pg:  0, off: 39)-->phy: 58407 (frm: 228) (prv: 227)--> val:    9 == value:    9 --  + ----> pg_fault
// log:   328 0x0148 (pg:  1, off: 72)-->phy: 57160 (frm: 223) (prv: 228)--> val:    0 == value:    0 --  +    HIT!
// log: 47868 0xbafc (pg:186, off:252)-->phy: 56572 (frm: 220) (prv: 228)--> val:    0 == value:    0 --  +    HIT!
// log: 13009 0x32d1 (pg: 50, off:209)-->phy: 30417 (frm: 118) (prv: 228)--> val:    0 == value:    0 --  +    HIT!
// log: 22378 0x576a (pg: 87, off:106)-->phy: 11370 (frm:  44) (prv: 228)--> val:   21 == value:   21 --  +    HIT!

// log: 39304 0x9988 (pg:153, off:136)-->phy: 55688 (frm: 217) (prv: 228)--> val:    0 == value:    0 --  +    HIT!
// log: 11171 0x2ba3 (pg: 43, off:163)-->phy: 40099 (frm: 156) (prv: 228)--> val:  -24 == value:  -24 --  +    HIT!
// log:  8079 0x1f8f (pg: 31, off:143)-->phy: 33935 (frm: 132) (prv: 228)--> val:  -29 == value:  -29 --  +    HIT!
// log: 52879 0xce8f (pg:206, off:143)-->phy: 47759 (frm: 186) (prv: 228)--> val:  -93 == value:  -93 --  +    HIT!
// log:  5123 0x1403 (pg: 20, off:  3)-->phy: 15107 (frm:  59) (prv: 228)--> val:    0 == value:    0 --  +    HIT!

// log:  4356 0x1104 (pg: 17, off:  4)-->phy: 58628 (frm: 229) (prv: 228)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 45745 0xb2b1 (pg:178, off:177)-->phy:  7857 (frm:  30) (prv: 229)--> val:    0 == value:    0 --  +    HIT!
// log: 32952 0x80b8 (pg:128, off:184)-->phy:  4280 (frm:  16) (prv: 229)--> val:    0 == value:    0 --  +    HIT!
// log:  4657 0x1231 (pg: 18, off: 49)-->phy: 27697 (frm: 108) (prv: 229)--> val:    0 == value:    0 --  +    HIT!
// log: 24142 0x5e4e (pg: 94, off: 78)-->phy: 33614 (frm: 131) (prv: 229)--> val:   23 == value:   23 --  +    HIT!

// log: 23319 0x5b17 (pg: 91, off: 23)-->phy: 20503 (frm:  80) (prv: 229)--> val:  -59 == value:  -59 --  +    HIT!
// log: 13607 0x3527 (pg: 53, off: 39)-->phy: 53031 (frm: 207) (prv: 229)--> val:   73 == value:   73 --  +    HIT!
// log: 46304 0xb4e0 (pg:180, off:224)-->phy: 50144 (frm: 195) (prv: 229)--> val:    0 == value:    0 --  +    HIT!
// log: 17677 0x450d (pg: 69, off: 13)-->phy:  9741 (frm:  38) (prv: 229)--> val:    0 == value:    0 --  +    HIT!
// log: 59691 0xe92b (pg:233, off: 43)-->phy: 47403 (frm: 185) (prv: 229)--> val:   74 == value:   74 --  +    HIT!

// log: 50967 0xc717 (pg:199, off: 23)-->phy: 42775 (frm: 167) (prv: 229)--> val:  -59 == value:  -59 --  +    HIT!
// log:  7817 0x1e89 (pg: 30, off:137)-->phy: 57993 (frm: 226) (prv: 229)--> val:    0 == value:    0 --  +    HIT!
// log:  8545 0x2161 (pg: 33, off: 97)-->phy: 20065 (frm:  78) (prv: 229)--> val:    0 == value:    0 --  +    HIT!
// log: 55297 0xd801 (pg:216, off:  1)-->phy: 58113 (frm: 227) (prv: 229)--> val:    0 == value:    0 --  +    HIT!
// log: 52954 0xceda (pg:206, off:218)-->phy: 47834 (frm: 186) (prv: 229)--> val:   51 == value:   51 --  +    HIT!

// log: 39720 0x9b28 (pg:155, off: 40)-->phy: 38184 (frm: 149) (prv: 229)--> val:    0 == value:    0 --  +    HIT!
// log: 18455 0x4817 (pg: 72, off: 23)-->phy:  5399 (frm:  21) (prv: 229)--> val:    5 == value:    5 --  +    HIT!
// log: 30349 0x768d (pg:118, off:141)-->phy: 59021 (frm: 230) (prv: 229)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 63270 0xf726 (pg:247, off: 38)-->phy: 16934 (frm:  66) (prv: 230)--> val:   61 == value:   61 --  +    HIT!
// log: 27156 0x6a14 (pg:106, off: 20)-->phy: 28692 (frm: 112) (prv: 230)--> val:    0 == value:    0 --  +    HIT!

// log: 20614 0x5086 (pg: 80, off:134)-->phy: 56966 (frm: 222) (prv: 230)--> val:   20 == value:   20 --  +    HIT!
// log: 19372 0x4bac (pg: 75, off:172)-->phy: 14252 (frm:  55) (prv: 230)--> val:    0 == value:    0 --  +    HIT!
// log: 48689 0xbe31 (pg:190, off: 49)-->phy: 32817 (frm: 128) (prv: 230)--> val:    0 == value:    0 --  +    HIT!
// log: 49386 0xc0ea (pg:192, off:234)-->phy:  8682 (frm:  33) (prv: 230)--> val:   48 == value:   48 --  +    HIT!
// log: 50584 0xc598 (pg:197, off:152)-->phy: 18584 (frm:  72) (prv: 230)--> val:    0 == value:    0 --  +    HIT!

// log: 51936 0xcae0 (pg:202, off:224)-->phy: 27360 (frm: 106) (prv: 230)--> val:    0 == value:    0 --  +    HIT!
// log: 34705 0x8791 (pg:135, off:145)-->phy: 18065 (frm:  70) (prv: 230)--> val:    0 == value:    0 --  +    HIT!
// log: 13653 0x3555 (pg: 53, off: 85)-->phy: 53077 (frm: 207) (prv: 230)--> val:    0 == value:    0 --  +    HIT!
// log: 50077 0xc39d (pg:195, off:157)-->phy: 59293 (frm: 231) (prv: 230)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 54518 0xd4f6 (pg:212, off:246)-->phy: 19446 (frm:  75) (prv: 231)--> val:   53 == value:   53 --  +    HIT!

// log: 41482 0xa20a (pg:162, off: 10)-->phy: 41738 (frm: 163) (prv: 231)--> val:   40 == value:   40 --  +    HIT!
// log:  4169 0x1049 (pg: 16, off: 73)-->phy: 50761 (frm: 198) (prv: 231)--> val:    0 == value:    0 --  +    HIT!
// log: 36118 0x8d16 (pg:141, off: 22)-->phy: 54550 (frm: 213) (prv: 231)--> val:   35 == value:   35 --  +    HIT!
// log:  9584 0x2570 (pg: 37, off:112)-->phy: 59504 (frm: 232) (prv: 231)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 18490 0x483a (pg: 72, off: 58)-->phy:  5434 (frm:  21) (prv: 232)--> val:   18 == value:   18 --  +    HIT!

// log: 55420 0xd87c (pg:216, off:124)-->phy: 58236 (frm: 227) (prv: 232)--> val:    0 == value:    0 --  +    HIT!
// log:  5708 0x164c (pg: 22, off: 76)-->phy: 52044 (frm: 203) (prv: 232)--> val:    0 == value:    0 --  +    HIT!
// log: 23506 0x5bd2 (pg: 91, off:210)-->phy: 20690 (frm:  80) (prv: 232)--> val:   22 == value:   22 --  +    HIT!
// log: 15391 0x3c1f (pg: 60, off: 31)-->phy: 31775 (frm: 124) (prv: 232)--> val:    7 == value:    7 --  +    HIT!
// log: 36368 0x8e10 (pg:142, off: 16)-->phy: 14352 (frm:  56) (prv: 232)--> val:    0 == value:    0 --  +    HIT!

// log: 38976 0x9840 (pg:152, off: 64)-->phy:  3904 (frm:  15) (prv: 232)--> val:    0 == value:    0 --  +    HIT!
// log: 50406 0xc4e6 (pg:196, off:230)-->phy: 14054 (frm:  54) (prv: 232)--> val:   49 == value:   49 --  +    HIT!
// log: 49236 0xc054 (pg:192, off: 84)-->phy:  8532 (frm:  33) (prv: 232)--> val:    0 == value:    0 --  +    HIT!
// log: 65035 0xfe0b (pg:254, off: 11)-->phy: 48395 (frm: 189) (prv: 232)--> val: -126 == value: -126 --  +    HIT!
// log: 30120 0x75a8 (pg:117, off:168)-->phy:   680 (frm:   2) (prv: 232)--> val:    0 == value:    0 --  +    HIT!

// log: 62551 0xf457 (pg:244, off: 87)-->phy:   343 (frm:   1) (prv: 232)--> val:   21 == value:   21 --  +    HIT!
// log: 46809 0xb6d9 (pg:182, off:217)-->phy: 55513 (frm: 216) (prv: 232)--> val:    0 == value:    0 --  +    HIT!
// log: 21687 0x54b7 (pg: 84, off:183)-->phy: 52407 (frm: 204) (prv: 232)--> val:   45 == value:   45 --  +    HIT!
// log: 53839 0xd24f (pg:210, off: 79)-->phy: 54095 (frm: 211) (prv: 232)--> val: -109 == value: -109 --  +    HIT!
// log:  2098 0x0832 (pg:  8, off: 50)-->phy: 35122 (frm: 137) (prv: 232)--> val:    2 == value:    2 --  +    HIT!

// log: 12364 0x304c (pg: 48, off: 76)-->phy: 45132 (frm: 176) (prv: 232)--> val:    0 == value:    0 --  +    HIT!
// log: 45366 0xb136 (pg:177, off: 54)-->phy: 45878 (frm: 179) (prv: 232)--> val:   44 == value:   44 --  +    HIT!
// log: 50437 0xc505 (pg:197, off:  5)-->phy: 18437 (frm:  72) (prv: 232)--> val:    0 == value:    0 --  +    HIT!
// log: 36675 0x8f43 (pg:143, off: 67)-->phy: 35651 (frm: 139) (prv: 232)--> val:  -48 == value:  -48 --  +    HIT!
// log: 55382 0xd856 (pg:216, off: 86)-->phy: 58198 (frm: 227) (prv: 232)--> val:   54 == value:   54 --  +    HIT!

// log: 11846 0x2e46 (pg: 46, off: 70)-->phy: 59718 (frm: 233) (prv: 232)--> val:   11 == value:   11 --  + ----> pg_fault
// log: 49127 0xbfe7 (pg:191, off:231)-->phy: 50663 (frm: 197) (prv: 233)--> val:   -7 == value:   -7 --  +    HIT!
// log: 19900 0x4dbc (pg: 77, off:188)-->phy: 36540 (frm: 142) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 20554 0x504a (pg: 80, off: 74)-->phy: 56906 (frm: 222) (prv: 233)--> val:   20 == value:   20 --  +    HIT!
// log: 19219 0x4b13 (pg: 75, off: 19)-->phy: 14099 (frm:  55) (prv: 233)--> val:  -60 == value:  -60 --  +    HIT!

// log: 51483 0xc91b (pg:201, off: 27)-->phy: 15899 (frm:  62) (prv: 233)--> val:   70 == value:   70 --  +    HIT!
// log: 58090 0xe2ea (pg:226, off:234)-->phy:  3306 (frm:  12) (prv: 233)--> val:   56 == value:   56 --  +    HIT!
// log: 39074 0x98a2 (pg:152, off:162)-->phy:  4002 (frm:  15) (prv: 233)--> val:   38 == value:   38 --  +    HIT!
// log: 16060 0x3ebc (pg: 62, off:188)-->phy: 21180 (frm:  82) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 10447 0x28cf (pg: 40, off:207)-->phy: 14799 (frm:  57) (prv: 233)--> val:   51 == value:   51 --  +    HIT!

// log: 54169 0xd399 (pg:211, off:153)-->phy: 51865 (frm: 202) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 20634 0x509a (pg: 80, off:154)-->phy: 56986 (frm: 222) (prv: 233)--> val:   20 == value:   20 --  +    HIT!
// log: 57555 0xe0d3 (pg:224, off:211)-->phy: 38099 (frm: 148) (prv: 233)--> val:   52 == value:   52 --  +    HIT!
// log: 61210 0xef1a (pg:239, off: 26)-->phy: 42266 (frm: 165) (prv: 233)--> val:   59 == value:   59 --  +    HIT!
// log:   269 0x010d (pg:  1, off: 13)-->phy: 57101 (frm: 223) (prv: 233)--> val:    0 == value:    0 --  +    HIT!

// log: 33154 0x8182 (pg:129, off:130)-->phy: 53634 (frm: 209) (prv: 233)--> val:   32 == value:   32 --  +    HIT!
// log: 64487 0xfbe7 (pg:251, off:231)-->phy:  5095 (frm:  19) (prv: 233)--> val:   -7 == value:   -7 --  +    HIT!
// log: 61223 0xef27 (pg:239, off: 39)-->phy: 42279 (frm: 165) (prv: 233)--> val:  -55 == value:  -55 --  +    HIT!
// log: 47292 0xb8bc (pg:184, off:188)-->phy: 29884 (frm: 116) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 21852 0x555c (pg: 85, off: 92)-->phy: 24156 (frm:  94) (prv: 233)--> val:    0 == value:    0 --  +    HIT!

// log:  5281 0x14a1 (pg: 20, off:161)-->phy: 15265 (frm:  59) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 45912 0xb358 (pg:179, off: 88)-->phy: 51032 (frm: 199) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 32532 0x7f14 (pg:127, off: 20)-->phy:  9492 (frm:  37) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 63067 0xf65b (pg:246, off: 91)-->phy: 22107 (frm:  86) (prv: 233)--> val: -106 == value: -106 --  +    HIT!
// log: 41683 0xa2d3 (pg:162, off:211)-->phy: 41939 (frm: 163) (prv: 233)--> val:  -76 == value:  -76 --  +    HIT!

// log: 20981 0x51f5 (pg: 81, off:245)-->phy: 39157 (frm: 152) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 33881 0x8459 (pg:132, off: 89)-->phy: 42073 (frm: 164) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 41785 0xa339 (pg:163, off: 57)-->phy: 38713 (frm: 151) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log:  4580 0x11e4 (pg: 17, off:228)-->phy: 58852 (frm: 229) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 41389 0xa1ad (pg:161, off:173)-->phy: 25005 (frm:  97) (prv: 233)--> val:    0 == value:    0 --  +    HIT!

// log: 28572 0x6f9c (pg:111, off:156)-->phy: 28060 (frm: 109) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log:   782 0x030e (pg:  3, off: 14)-->phy:  7950 (frm:  31) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 30273 0x7641 (pg:118, off: 65)-->phy: 58945 (frm: 230) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 62267 0xf33b (pg:243, off: 59)-->phy: 23867 (frm:  93) (prv: 233)--> val:  -50 == value:  -50 --  +    HIT!
// log: 17922 0x4602 (pg: 70, off:  2)-->phy: 18690 (frm:  73) (prv: 233)--> val:   17 == value:   17 --  +    HIT!

// log: 63238 0xf706 (pg:247, off:  6)-->phy: 16902 (frm:  66) (prv: 233)--> val:   61 == value:   61 --  +    HIT!
// log:  3308 0x0cec (pg: 12, off:236)-->phy: 40428 (frm: 157) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 26545 0x67b1 (pg:103, off:177)-->phy: 24497 (frm:  95) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 44395 0xad6b (pg:173, off:107)-->phy: 44907 (frm: 175) (prv: 233)--> val:   90 == value:   90 --  +    HIT!
// log: 39120 0x98d0 (pg:152, off:208)-->phy:  4048 (frm:  15) (prv: 233)--> val:    0 == value:    0 --  +    HIT!

// log: 21706 0x54ca (pg: 84, off:202)-->phy: 52426 (frm: 204) (prv: 233)--> val:   21 == value:   21 --  +    HIT!
// log:  7144 0x1be8 (pg: 27, off:232)-->phy: 26600 (frm: 103) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 30244 0x7624 (pg:118, off: 36)-->phy: 58916 (frm: 230) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log:  3725 0x0e8d (pg: 14, off:141)-->phy: 10125 (frm:  39) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 54632 0xd568 (pg:213, off:104)-->phy: 48744 (frm: 190) (prv: 233)--> val:    0 == value:    0 --  +    HIT!

// log: 30574 0x776e (pg:119, off:110)-->phy: 16238 (frm:  63) (prv: 233)--> val:   29 == value:   29 --  +    HIT!
// log:  8473 0x2119 (pg: 33, off: 25)-->phy: 19993 (frm:  78) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 12386 0x3062 (pg: 48, off: 98)-->phy: 45154 (frm: 176) (prv: 233)--> val:   12 == value:   12 --  +    HIT!
// log: 41114 0xa09a (pg:160, off:154)-->phy:  8858 (frm:  34) (prv: 233)--> val:   40 == value:   40 --  +    HIT!
// log: 57930 0xe24a (pg:226, off: 74)-->phy:  3146 (frm:  12) (prv: 233)--> val:   56 == value:   56 --  +    HIT!

// log: 15341 0x3bed (pg: 59, off:237)-->phy: 32493 (frm: 126) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 15598 0x3cee (pg: 60, off:238)-->phy: 31982 (frm: 124) (prv: 233)--> val:   15 == value:   15 --  +    HIT!
// log: 59922 0xea12 (pg:234, off: 18)-->phy: 10514 (frm:  41) (prv: 233)--> val:   58 == value:   58 --  +    HIT!
// log: 18226 0x4732 (pg: 71, off: 50)-->phy:  2354 (frm:   9) (prv: 233)--> val:   17 == value:   17 --  +    HIT!
// log: 48162 0xbc22 (pg:188, off: 34)-->phy: 17698 (frm:  69) (prv: 233)--> val:   47 == value:   47 --  +    HIT!

// log: 41250 0xa122 (pg:161, off: 34)-->phy: 24866 (frm:  97) (prv: 233)--> val:   40 == value:   40 --  +    HIT!
// log:  1512 0x05e8 (pg:  5, off:232)-->phy: 51688 (frm: 201) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log:  2546 0x09f2 (pg:  9, off:242)-->phy:  4850 (frm:  18) (prv: 233)--> val:    2 == value:    2 --  +    HIT!
// log: 41682 0xa2d2 (pg:162, off:210)-->phy: 41938 (frm: 163) (prv: 233)--> val:   40 == value:   40 --  +    HIT!
// log:   322 0x0142 (pg:  1, off: 66)-->phy: 57154 (frm: 223) (prv: 233)--> val:    0 == value:    0 --  +    HIT!

// log:   880 0x0370 (pg:  3, off:112)-->phy:  8048 (frm:  31) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 20891 0x519b (pg: 81, off:155)-->phy: 39067 (frm: 152) (prv: 233)--> val:  102 == value:  102 --  +    HIT!
// log: 56604 0xdd1c (pg:221, off: 28)-->phy: 13340 (frm:  52) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 40166 0x9ce6 (pg:156, off:230)-->phy:  1254 (frm:   4) (prv: 233)--> val:   39 == value:   39 --  +    HIT!
// log: 26791 0x68a7 (pg:104, off:167)-->phy: 42663 (frm: 166) (prv: 233)--> val:   41 == value:   41 --  +    HIT!

// log: 44560 0xae10 (pg:174, off: 16)-->phy: 11024 (frm:  43) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 38698 0x972a (pg:151, off: 42)-->phy: 46890 (frm: 183) (prv: 233)--> val:   37 == value:   37 --  +    HIT!
// log: 64127 0xfa7f (pg:250, off:127)-->phy:  4479 (frm:  17) (prv: 233)--> val:  -97 == value:  -97 --  +    HIT!
// log: 15028 0x3ab4 (pg: 58, off:180)-->phy: 24756 (frm:  96) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 38669 0x970d (pg:151, off: 13)-->phy: 46861 (frm: 183) (prv: 233)--> val:    0 == value:    0 --  +    HIT!

// log: 45637 0xb245 (pg:178, off: 69)-->phy:  7749 (frm:  30) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 43151 0xa88f (pg:168, off:143)-->phy: 17551 (frm:  68) (prv: 233)--> val:   35 == value:   35 --  +    HIT!
// log:  9465 0x24f9 (pg: 36, off:249)-->phy: 23033 (frm:  89) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log:  2498 0x09c2 (pg:  9, off:194)-->phy:  4802 (frm:  18) (prv: 233)--> val:    2 == value:    2 --  +    HIT!
// log: 13978 0x369a (pg: 54, off:154)-->phy: 31386 (frm: 122) (prv: 233)--> val:   13 == value:   13 --  +    HIT!

// log: 16326 0x3fc6 (pg: 63, off:198)-->phy: 56006 (frm: 218) (prv: 233)--> val:   15 == value:   15 --  +    HIT!
// log: 51442 0xc8f2 (pg:200, off:242)-->phy: 32754 (frm: 127) (prv: 233)--> val:   50 == value:   50 --  +    HIT!
// log: 34845 0x881d (pg:136, off: 29)-->phy: 26653 (frm: 104) (prv: 233)--> val:    0 == value:    0 --  +    HIT!
// log: 63667 0xf8b3 (pg:248, off:179)-->phy: 28339 (frm: 110) (prv: 233)--> val:   44 == value:   44 --  +    HIT!
// log: 39370 0x99ca (pg:153, off:202)-->phy: 55754 (frm: 217) (prv: 233)--> val:   38 == value:   38 --  +    HIT!

// log: 55671 0xd977 (pg:217, off:119)-->phy: 60023 (frm: 234) (prv: 233)--> val:   93 == value:   93 --  + ----> pg_fault
// log: 64496 0xfbf0 (pg:251, off:240)-->phy:  5104 (frm:  19) (prv: 234)--> val:    0 == value:    0 --  +    HIT!
// log:  7767 0x1e57 (pg: 30, off: 87)-->phy: 57943 (frm: 226) (prv: 234)--> val: -107 == value: -107 --  +    HIT!
// log:  6283 0x188b (pg: 24, off:139)-->phy:  7563 (frm:  29) (prv: 234)--> val:   34 == value:   34 --  +    HIT!
// log: 55884 0xda4c (pg:218, off: 76)-->phy: 34636 (frm: 135) (prv: 234)--> val:    0 == value:    0 --  +    HIT!

// log: 61103 0xeeaf (pg:238, off:175)-->phy:  6063 (frm:  23) (prv: 234)--> val:  -85 == value:  -85 --  +    HIT!
// log: 10184 0x27c8 (pg: 39, off:200)-->phy: 60360 (frm: 235) (prv: 234)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 39543 0x9a77 (pg:154, off:119)-->phy: 43383 (frm: 169) (prv: 235)--> val:  -99 == value:  -99 --  +    HIT!
// log:  9555 0x2553 (pg: 37, off: 83)-->phy: 59475 (frm: 232) (prv: 235)--> val:   84 == value:   84 --  +    HIT!
// log: 13963 0x368b (pg: 54, off:139)-->phy: 31371 (frm: 122) (prv: 235)--> val:  -94 == value:  -94 --  +    HIT!

// log: 58975 0xe65f (pg:230, off: 95)-->phy: 14943 (frm:  58) (prv: 235)--> val: -105 == value: -105 --  +    HIT!
// log: 19537 0x4c51 (pg: 76, off: 81)-->phy: 22609 (frm:  88) (prv: 235)--> val:    0 == value:    0 --  +    HIT!
// log:  6101 0x17d5 (pg: 23, off:213)-->phy:  9429 (frm:  36) (prv: 235)--> val:    0 == value:    0 --  +    HIT!
// log: 41421 0xa1cd (pg:161, off:205)-->phy: 25037 (frm:  97) (prv: 235)--> val:    0 == value:    0 --  +    HIT!
// log: 45502 0xb1be (pg:177, off:190)-->phy: 46014 (frm: 179) (prv: 235)--> val:   44 == value:   44 --  +    HIT!

// log: 29328 0x7290 (pg:114, off:144)-->phy: 37008 (frm: 144) (prv: 235)--> val:    0 == value:    0 --  +    HIT!
// log:  8149 0x1fd5 (pg: 31, off:213)-->phy: 34005 (frm: 132) (prv: 235)--> val:    0 == value:    0 --  +    HIT!
// log: 25450 0x636a (pg: 99, off:106)-->phy: 60522 (frm: 236) (prv: 235)--> val:   24 == value:   24 --  + ----> pg_fault
// log: 58944 0xe640 (pg:230, off: 64)-->phy: 14912 (frm:  58) (prv: 236)--> val:    0 == value:    0 --  +    HIT!
// log: 50666 0xc5ea (pg:197, off:234)-->phy: 18666 (frm:  72) (prv: 236)--> val:   49 == value:   49 --  +    HIT!

// log: 23084 0x5a2c (pg: 90, off: 44)-->phy: 35884 (frm: 140) (prv: 236)--> val:    0 == value:    0 --  +    HIT!
// log: 36468 0x8e74 (pg:142, off:116)-->phy: 14452 (frm:  56) (prv: 236)--> val:    0 == value:    0 --  +    HIT!
// log: 33645 0x836d (pg:131, off:109)-->phy: 60781 (frm: 237) (prv: 236)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 25002 0x61aa (pg: 97, off:170)-->phy: 52650 (frm: 205) (prv: 237)--> val:   24 == value:   24 --  +    HIT!
// log: 53715 0xd1d3 (pg:209, off:211)-->phy:   979 (frm:   3) (prv: 237)--> val:  116 == value:  116 --  +    HIT!

// log: 60173 0xeb0d (pg:235, off: 13)-->phy: 57613 (frm: 225) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 46354 0xb512 (pg:181, off: 18)-->phy: 50194 (frm: 196) (prv: 237)--> val:   45 == value:   45 --  +    HIT!
// log:  4708 0x1264 (pg: 18, off:100)-->phy: 27748 (frm: 108) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 28208 0x6e30 (pg:110, off: 48)-->phy: 34864 (frm: 136) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 58844 0xe5dc (pg:229, off:220)-->phy: 55260 (frm: 215) (prv: 237)--> val:    0 == value:    0 --  +    HIT!

// log: 22173 0x569d (pg: 86, off:157)-->phy: 56733 (frm: 221) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log:  8535 0x2157 (pg: 33, off: 87)-->phy: 20055 (frm:  78) (prv: 237)--> val:   85 == value:   85 --  +    HIT!
// log: 42261 0xa515 (pg:165, off: 21)-->phy: 10773 (frm:  42) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 29687 0x73f7 (pg:115, off:247)-->phy: 49655 (frm: 193) (prv: 237)--> val:   -3 == value:   -3 --  +    HIT!
// log: 37799 0x93a7 (pg:147, off:167)-->phy: 29351 (frm: 114) (prv: 237)--> val:  -23 == value:  -23 --  +    HIT!

// log: 22566 0x5826 (pg: 88, off: 38)-->phy:  2854 (frm:  11) (prv: 237)--> val:   22 == value:   22 --  +    HIT!
// log: 62520 0xf438 (pg:244, off: 56)-->phy:   312 (frm:   1) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log:  4098 0x1002 (pg: 16, off:  2)-->phy: 50690 (frm: 198) (prv: 237)--> val:    4 == value:    4 --  +    HIT!
// log: 47999 0xbb7f (pg:187, off:127)-->phy: 25727 (frm: 100) (prv: 237)--> val:  -33 == value:  -33 --  +    HIT!
// log: 49660 0xc1fc (pg:193, off:252)-->phy: 45564 (frm: 177) (prv: 237)--> val:    0 == value:    0 --  +    HIT!

// log: 37063 0x90c7 (pg:144, off:199)-->phy: 18375 (frm:  71) (prv: 237)--> val:   49 == value:   49 --  +    HIT!
// log: 41856 0xa380 (pg:163, off:128)-->phy: 38784 (frm: 151) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log:  5417 0x1529 (pg: 21, off: 41)-->phy: 21545 (frm:  84) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 48856 0xbed8 (pg:190, off:216)-->phy: 32984 (frm: 128) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 10682 0x29ba (pg: 41, off:186)-->phy: 27578 (frm: 107) (prv: 237)--> val:   10 == value:   10 --  +    HIT!

// log: 22370 0x5762 (pg: 87, off: 98)-->phy: 11362 (frm:  44) (prv: 237)--> val:   21 == value:   21 --  +    HIT!
// log: 63281 0xf731 (pg:247, off: 49)-->phy: 16945 (frm:  66) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 62452 0xf3f4 (pg:243, off:244)-->phy: 24052 (frm:  93) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 50532 0xc564 (pg:197, off:100)-->phy: 18532 (frm:  72) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log:  9022 0x233e (pg: 35, off: 62)-->phy: 48190 (frm: 188) (prv: 237)--> val:    8 == value:    8 --  +    HIT!

// log: 59300 0xe7a4 (pg:231, off:164)-->phy: 10404 (frm:  40) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 58660 0xe524 (pg:229, off: 36)-->phy: 55076 (frm: 215) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 56401 0xdc51 (pg:220, off: 81)-->phy: 51281 (frm: 200) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log:  8518 0x2146 (pg: 33, off: 70)-->phy: 20038 (frm:  78) (prv: 237)--> val:    8 == value:    8 --  +    HIT!
// log: 63066 0xf65a (pg:246, off: 90)-->phy: 22106 (frm:  86) (prv: 237)--> val:   61 == value:   61 --  +    HIT!

// log: 63250 0xf712 (pg:247, off: 18)-->phy: 16914 (frm:  66) (prv: 237)--> val:   61 == value:   61 --  +    HIT!
// log: 48592 0xbdd0 (pg:189, off:208)-->phy:  2000 (frm:   7) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 28771 0x7063 (pg:112, off: 99)-->phy:  1379 (frm:   5) (prv: 237)--> val:   24 == value:   24 --  +    HIT!
// log: 37673 0x9329 (pg:147, off: 41)-->phy: 29225 (frm: 114) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 60776 0xed68 (pg:237, off:104)-->phy: 23656 (frm:  92) (prv: 237)--> val:    0 == value:    0 --  +    HIT!

// log: 56438 0xdc76 (pg:220, off:118)-->phy: 51318 (frm: 200) (prv: 237)--> val:   55 == value:   55 --  +    HIT!
// log: 60424 0xec08 (pg:236, off:  8)-->phy:  7176 (frm:  28) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 39993 0x9c39 (pg:156, off: 57)-->phy:  1081 (frm:   4) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 56004 0xdac4 (pg:218, off:196)-->phy: 34756 (frm: 135) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 59002 0xe67a (pg:230, off:122)-->phy: 14970 (frm:  58) (prv: 237)--> val:   57 == value:   57 --  +    HIT!

// log: 33982 0x84be (pg:132, off:190)-->phy: 42174 (frm: 164) (prv: 237)--> val:   33 == value:   33 --  +    HIT!
// log: 25498 0x639a (pg: 99, off:154)-->phy: 60570 (frm: 236) (prv: 237)--> val:   24 == value:   24 --  +    HIT!
// log: 57047 0xded7 (pg:222, off:215)-->phy: 37591 (frm: 146) (prv: 237)--> val:  -75 == value:  -75 --  +    HIT!
// log:  1401 0x0579 (pg:  5, off:121)-->phy: 51577 (frm: 201) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 15130 0x3b1a (pg: 59, off: 26)-->phy: 32282 (frm: 126) (prv: 237)--> val:   14 == value:   14 --  +    HIT!

// log: 42960 0xa7d0 (pg:167, off:208)-->phy: 19664 (frm:  76) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 61827 0xf183 (pg:241, off:131)-->phy: 22403 (frm:  87) (prv: 237)--> val:   96 == value:   96 --  +    HIT!
// log: 32442 0x7eba (pg:126, off:186)-->phy:  7098 (frm:  27) (prv: 237)--> val:   31 == value:   31 --  +    HIT!
// log: 64304 0xfb30 (pg:251, off: 48)-->phy:  4912 (frm:  19) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 30273 0x7641 (pg:118, off: 65)-->phy: 58945 (frm: 230) (prv: 237)--> val:    0 == value:    0 --  +    HIT!

// log: 38082 0x94c2 (pg:148, off:194)-->phy: 37314 (frm: 145) (prv: 237)--> val:   37 == value:   37 --  +    HIT!
// log: 22404 0x5784 (pg: 87, off:132)-->phy: 11396 (frm:  44) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log:  3808 0x0ee0 (pg: 14, off:224)-->phy: 10208 (frm:  39) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 16883 0x41f3 (pg: 65, off:243)-->phy: 48115 (frm: 187) (prv: 237)--> val:  124 == value:  124 --  +    HIT!
// log: 23111 0x5a47 (pg: 90, off: 71)-->phy: 35911 (frm: 140) (prv: 237)--> val: -111 == value: -111 --  +    HIT!

// log: 62417 0xf3d1 (pg:243, off:209)-->phy: 24017 (frm:  93) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 60364 0xebcc (pg:235, off:204)-->phy: 57804 (frm: 225) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log:  4542 0x11be (pg: 17, off:190)-->phy: 58814 (frm: 229) (prv: 237)--> val:    4 == value:    4 --  +    HIT!
// log: 14829 0x39ed (pg: 57, off:237)-->phy: 33517 (frm: 130) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 44964 0xafa4 (pg:175, off:164)-->phy: 13220 (frm:  51) (prv: 237)--> val:    0 == value:    0 --  +    HIT!

// log: 33924 0x8484 (pg:132, off:132)-->phy: 42116 (frm: 164) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log:  2141 0x085d (pg:  8, off: 93)-->phy: 35165 (frm: 137) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 19245 0x4b2d (pg: 75, off: 45)-->phy: 14125 (frm:  55) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 47168 0xb840 (pg:184, off: 64)-->phy: 29760 (frm: 116) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 24048 0x5df0 (pg: 93, off:240)-->phy: 39664 (frm: 154) (prv: 237)--> val:    0 == value:    0 --  +    HIT!

// log:  1022 0x03fe (pg:  3, off:254)-->phy:  8190 (frm:  31) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 23075 0x5a23 (pg: 90, off: 35)-->phy: 35875 (frm: 140) (prv: 237)--> val: -120 == value: -120 --  +    HIT!
// log: 24888 0x6138 (pg: 97, off: 56)-->phy: 52536 (frm: 205) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 49247 0xc05f (pg:192, off: 95)-->phy:  8543 (frm:  33) (prv: 237)--> val:   23 == value:   23 --  +    HIT!
// log:  4900 0x1324 (pg: 19, off: 36)-->phy: 13604 (frm:  53) (prv: 237)--> val:    0 == value:    0 --  +    HIT!

// log: 22656 0x5880 (pg: 88, off:128)-->phy:  2944 (frm:  11) (prv: 237)--> val:    0 == value:    0 --  +    HIT!
// log: 34117 0x8545 (pg:133, off: 69)-->phy: 60997 (frm: 238) (prv: 237)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 55555 0xd903 (pg:217, off:  3)-->phy: 59907 (frm: 234) (prv: 238)--> val:   64 == value:   64 --  +    HIT!
// log: 48947 0xbf33 (pg:191, off: 51)-->phy: 50483 (frm: 197) (prv: 238)--> val:  -52 == value:  -52 --  +    HIT!
// log: 59533 0xe88d (pg:232, off:141)-->phy: 25997 (frm: 101) (prv: 238)--> val:    0 == value:    0 --  +    HIT!

// log: 21312 0x5340 (pg: 83, off: 64)-->phy:  9024 (frm:  35) (prv: 238)--> val:    0 == value:    0 --  +    HIT!
// log: 21415 0x53a7 (pg: 83, off:167)-->phy:  9127 (frm:  35) (prv: 238)--> val:  -23 == value:  -23 --  +    HIT!
// log:   813 0x032d (pg:  3, off: 45)-->phy:  7981 (frm:  31) (prv: 238)--> val:    0 == value:    0 --  +    HIT!
// log: 19419 0x4bdb (pg: 75, off:219)-->phy: 14299 (frm:  55) (prv: 238)--> val:  -10 == value:  -10 --  +    HIT!
// log:  1999 0x07cf (pg:  7, off:207)-->phy: 33231 (frm: 129) (prv: 238)--> val:  -13 == value:  -13 --  +    HIT!

// log: 20155 0x4ebb (pg: 78, off:187)-->phy: 40635 (frm: 158) (prv: 238)--> val:  -82 == value:  -82 --  +    HIT!
// log: 21521 0x5411 (pg: 84, off: 17)-->phy: 52241 (frm: 204) (prv: 238)--> val:    0 == value:    0 --  +    HIT!
// log: 13670 0x3566 (pg: 53, off:102)-->phy: 53094 (frm: 207) (prv: 238)--> val:   13 == value:   13 --  +    HIT!
// log: 19289 0x4b59 (pg: 75, off: 89)-->phy: 14169 (frm:  55) (prv: 238)--> val:    0 == value:    0 --  +    HIT!
// log: 58483 0xe473 (pg:228, off:115)-->phy: 15475 (frm:  60) (prv: 238)--> val:   28 == value:   28 --  +    HIT!

// log: 41318 0xa166 (pg:161, off:102)-->phy: 24934 (frm:  97) (prv: 238)--> val:   40 == value:   40 --  +    HIT!
// log: 16151 0x3f17 (pg: 63, off: 23)-->phy: 55831 (frm: 218) (prv: 238)--> val:  -59 == value:  -59 --  +    HIT!
// log: 13611 0x352b (pg: 53, off: 43)-->phy: 53035 (frm: 207) (prv: 238)--> val:   74 == value:   74 --  +    HIT!
// log: 21514 0x540a (pg: 84, off: 10)-->phy: 52234 (frm: 204) (prv: 238)--> val:   21 == value:   21 --  +    HIT!
// log: 13499 0x34bb (pg: 52, off:187)-->phy: 36283 (frm: 141) (prv: 238)--> val:   46 == value:   46 --  +    HIT!

// log: 45583 0xb20f (pg:178, off: 15)-->phy:  7695 (frm:  30) (prv: 238)--> val: -125 == value: -125 --  +    HIT!
// log: 49013 0xbf75 (pg:191, off:117)-->phy: 50549 (frm: 197) (prv: 238)--> val:    0 == value:    0 --  +    HIT!
// log: 64843 0xfd4b (pg:253, off: 75)-->phy:  2123 (frm:   8) (prv: 238)--> val:   82 == value:   82 --  +    HIT!
// log: 63485 0xf7fd (pg:247, off:253)-->phy: 17149 (frm:  66) (prv: 238)--> val:    0 == value:    0 --  +    HIT!
// log: 38697 0x9729 (pg:151, off: 41)-->phy: 46889 (frm: 183) (prv: 238)--> val:    0 == value:    0 --  +    HIT!

// log: 59188 0xe734 (pg:231, off: 52)-->phy: 10292 (frm:  40) (prv: 238)--> val:    0 == value:    0 --  +    HIT!
// log: 24593 0x6011 (pg: 96, off: 17)-->phy: 38417 (frm: 150) (prv: 238)--> val:    0 == value:    0 --  +    HIT!
// log: 57641 0xe129 (pg:225, off: 41)-->phy: 61225 (frm: 239) (prv: 238)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 36524 0x8eac (pg:142, off:172)-->phy: 14508 (frm:  56) (prv: 239)--> val:    0 == value:    0 --  +    HIT!
// log: 56980 0xde94 (pg:222, off:148)-->phy: 37524 (frm: 146) (prv: 239)--> val:    0 == value:    0 --  +    HIT!

// log: 36810 0x8fca (pg:143, off:202)-->phy: 35786 (frm: 139) (prv: 239)--> val:   35 == value:   35 --  +    HIT!
// log:  6096 0x17d0 (pg: 23, off:208)-->phy:  9424 (frm:  36) (prv: 239)--> val:    0 == value:    0 --  +    HIT!
// log: 11070 0x2b3e (pg: 43, off: 62)-->phy: 39998 (frm: 156) (prv: 239)--> val:   10 == value:   10 --  +    HIT!
// log: 60124 0xeadc (pg:234, off:220)-->phy: 10716 (frm:  41) (prv: 239)--> val:    0 == value:    0 --  +    HIT!
// log: 37576 0x92c8 (pg:146, off:200)-->phy: 21448 (frm:  83) (prv: 239)--> val:    0 == value:    0 --  +    HIT!

// log: 15096 0x3af8 (pg: 58, off:248)-->phy: 24824 (frm:  96) (prv: 239)--> val:    0 == value:    0 --  +    HIT!
// log: 45247 0xb0bf (pg:176, off:191)-->phy: 49087 (frm: 191) (prv: 239)--> val:   47 == value:   47 --  +    HIT!
// log: 32783 0x800f (pg:128, off: 15)-->phy:  4111 (frm:  16) (prv: 239)--> val:    3 == value:    3 --  +    HIT!
// log: 58390 0xe416 (pg:228, off: 22)-->phy: 15382 (frm:  60) (prv: 239)--> val:   57 == value:   57 --  +    HIT!
// log: 60873 0xedc9 (pg:237, off:201)-->phy: 23753 (frm:  92) (prv: 239)--> val:    0 == value:    0 --  +    HIT!

// log: 23719 0x5ca7 (pg: 92, off:167)-->phy: 54951 (frm: 214) (prv: 239)--> val:   41 == value:   41 --  +    HIT!
// log: 24385 0x5f41 (pg: 95, off: 65)-->phy:  1601 (frm:   6) (prv: 239)--> val:    0 == value:    0 --  +    HIT!
// log: 22307 0x5723 (pg: 87, off: 35)-->phy: 11299 (frm:  44) (prv: 239)--> val:  -56 == value:  -56 --  +    HIT!
// log: 17375 0x43df (pg: 67, off:223)-->phy: 61663 (frm: 240) (prv: 239)--> val:   -9 == value:   -9 --  + ----> pg_fault
// log: 15990 0x3e76 (pg: 62, off:118)-->phy: 21110 (frm:  82) (prv: 240)--> val:   15 == value:   15 --  +    HIT!

// log: 20526 0x502e (pg: 80, off: 46)-->phy: 56878 (frm: 222) (prv: 240)--> val:   20 == value:   20 --  +    HIT!
// log: 25904 0x6530 (pg:101, off: 48)-->phy: 36656 (frm: 143) (prv: 240)--> val:    0 == value:    0 --  +    HIT!
// log: 42224 0xa4f0 (pg:164, off:240)-->phy: 46320 (frm: 180) (prv: 240)--> val:    0 == value:    0 --  +    HIT!
// log:  9311 0x245f (pg: 36, off: 95)-->phy: 22879 (frm:  89) (prv: 240)--> val:   23 == value:   23 --  +    HIT!
// log:  7862 0x1eb6 (pg: 30, off:182)-->phy: 58038 (frm: 226) (prv: 240)--> val:    7 == value:    7 --  +    HIT!

// log:  3835 0x0efb (pg: 14, off:251)-->phy: 10235 (frm:  39) (prv: 240)--> val:  -66 == value:  -66 --  +    HIT!
// log: 30535 0x7747 (pg:119, off: 71)-->phy: 16199 (frm:  63) (prv: 240)--> val:  -47 == value:  -47 --  +    HIT!
// log: 65179 0xfe9b (pg:254, off:155)-->phy: 48539 (frm: 189) (prv: 240)--> val:  -90 == value:  -90 --  +    HIT!
// log: 57387 0xe02b (pg:224, off: 43)-->phy: 37931 (frm: 148) (prv: 240)--> val:   10 == value:   10 --  +    HIT!
// log: 63579 0xf85b (pg:248, off: 91)-->phy: 28251 (frm: 110) (prv: 240)--> val:   22 == value:   22 --  +    HIT!

// log:  4946 0x1352 (pg: 19, off: 82)-->phy: 13650 (frm:  53) (prv: 240)--> val:    4 == value:    4 --  +    HIT!
// log:  9037 0x234d (pg: 35, off: 77)-->phy: 48205 (frm: 188) (prv: 240)--> val:    0 == value:    0 --  +    HIT!
// log: 61033 0xee69 (pg:238, off:105)-->phy:  5993 (frm:  23) (prv: 240)--> val:    0 == value:    0 --  +    HIT!
// log: 55543 0xd8f7 (pg:216, off:247)-->phy: 58359 (frm: 227) (prv: 240)--> val:   61 == value:   61 --  +    HIT!
// log: 50361 0xc4b9 (pg:196, off:185)-->phy: 14009 (frm:  54) (prv: 240)--> val:    0 == value:    0 --  +    HIT!

// log:  6480 0x1950 (pg: 25, off: 80)-->phy: 29008 (frm: 113) (prv: 240)--> val:    0 == value:    0 --  +    HIT!
// log: 14042 0x36da (pg: 54, off:218)-->phy: 31450 (frm: 122) (prv: 240)--> val:   13 == value:   13 --  +    HIT!
// log: 21531 0x541b (pg: 84, off: 27)-->phy: 52251 (frm: 204) (prv: 240)--> val:    6 == value:    6 --  +    HIT!
// log: 39195 0x991b (pg:153, off: 27)-->phy: 55579 (frm: 217) (prv: 240)--> val:   70 == value:   70 --  +    HIT!
// log: 37511 0x9287 (pg:146, off:135)-->phy: 21383 (frm:  83) (prv: 240)--> val:  -95 == value:  -95 --  +    HIT!

// log: 23696 0x5c90 (pg: 92, off:144)-->phy: 54928 (frm: 214) (prv: 240)--> val:    0 == value:    0 --  +    HIT!
// log: 27440 0x6b30 (pg:107, off: 48)-->phy: 15664 (frm:  61) (prv: 240)--> val:    0 == value:    0 --  +    HIT!
// log: 28201 0x6e29 (pg:110, off: 41)-->phy: 34857 (frm: 136) (prv: 240)--> val:    0 == value:    0 --  +    HIT!
// log: 23072 0x5a20 (pg: 90, off: 32)-->phy: 35872 (frm: 140) (prv: 240)--> val:    0 == value:    0 --  +    HIT!
// log:  7814 0x1e86 (pg: 30, off:134)-->phy: 57990 (frm: 226) (prv: 240)--> val:    7 == value:    7 --  +    HIT!

// log:  6552 0x1998 (pg: 25, off:152)-->phy: 29080 (frm: 113) (prv: 240)--> val:    0 == value:    0 --  +    HIT!
// log: 43637 0xaa75 (pg:170, off:117)-->phy: 12661 (frm:  49) (prv: 240)--> val:    0 == value:    0 --  +    HIT!
// log: 35113 0x8929 (pg:137, off: 41)-->phy: 39721 (frm: 155) (prv: 240)--> val:    0 == value:    0 --  +    HIT!
// log: 34890 0x884a (pg:136, off: 74)-->phy: 26698 (frm: 104) (prv: 240)--> val:   34 == value:   34 --  +    HIT!
// log: 61297 0xef71 (pg:239, off:113)-->phy: 42353 (frm: 165) (prv: 240)--> val:    0 == value:    0 --  +    HIT!

// log: 45633 0xb241 (pg:178, off: 65)-->phy:  7745 (frm:  30) (prv: 240)--> val:    0 == value:    0 --  +    HIT!
// log: 61431 0xeff7 (pg:239, off:247)-->phy: 42487 (frm: 165) (prv: 240)--> val:   -3 == value:   -3 --  +    HIT!
// log: 46032 0xb3d0 (pg:179, off:208)-->phy: 51152 (frm: 199) (prv: 240)--> val:    0 == value:    0 --  +    HIT!
// log: 18774 0x4956 (pg: 73, off: 86)-->phy: 61782 (frm: 241) (prv: 240)--> val:   18 == value:   18 --  + ----> pg_fault
// log: 62991 0xf60f (pg:246, off: 15)-->phy: 22031 (frm:  86) (prv: 241)--> val: -125 == value: -125 --  +    HIT!

// log: 28059 0x6d9b (pg:109, off:155)-->phy:  3483 (frm:  13) (prv: 241)--> val:  102 == value:  102 --  +    HIT!
// log: 35229 0x899d (pg:137, off:157)-->phy: 39837 (frm: 155) (prv: 241)--> val:    0 == value:    0 --  +    HIT!
// log: 51230 0xc81e (pg:200, off: 30)-->phy: 32542 (frm: 127) (prv: 241)--> val:   50 == value:   50 --  +    HIT!
// log: 14405 0x3845 (pg: 56, off: 69)-->phy:  5701 (frm:  22) (prv: 241)--> val:    0 == value:    0 --  +    HIT!
// log: 52242 0xcc12 (pg:204, off: 18)-->phy: 46610 (frm: 182) (prv: 241)--> val:   51 == value:   51 --  +    HIT!

// log: 43153 0xa891 (pg:168, off:145)-->phy: 17553 (frm:  68) (prv: 241)--> val:    0 == value:    0 --  +    HIT!
// log:  2709 0x0a95 (pg: 10, off:149)-->phy: 53909 (frm: 210) (prv: 241)--> val:    0 == value:    0 --  +    HIT!
// log: 47963 0xbb5b (pg:187, off: 91)-->phy: 25691 (frm: 100) (prv: 241)--> val:  -42 == value:  -42 --  +    HIT!
// log: 36943 0x904f (pg:144, off: 79)-->phy: 18255 (frm:  71) (prv: 241)--> val:   19 == value:   19 --  +    HIT!
// log: 54066 0xd332 (pg:211, off: 50)-->phy: 51762 (frm: 202) (prv: 241)--> val:   52 == value:   52 --  +    HIT!

// log: 10054 0x2746 (pg: 39, off: 70)-->phy: 60230 (frm: 235) (prv: 241)--> val:    9 == value:    9 --  +    HIT!
// log: 43051 0xa82b (pg:168, off: 43)-->phy: 17451 (frm:  68) (prv: 241)--> val:   10 == value:   10 --  +    HIT!
// log: 11525 0x2d05 (pg: 45, off:  5)-->phy: 41477 (frm: 162) (prv: 241)--> val:    0 == value:    0 --  +    HIT!
// log: 17684 0x4514 (pg: 69, off: 20)-->phy:  9748 (frm:  38) (prv: 241)--> val:    0 == value:    0 --  +    HIT!
// log: 41681 0xa2d1 (pg:162, off:209)-->phy: 41937 (frm: 163) (prv: 241)--> val:    0 == value:    0 --  +    HIT!

// log: 27883 0x6ceb (pg:108, off:235)-->phy: 44267 (frm: 172) (prv: 241)--> val:   58 == value:   58 --  +    HIT!
// log: 56909 0xde4d (pg:222, off: 77)-->phy: 37453 (frm: 146) (prv: 241)--> val:    0 == value:    0 --  +    HIT!
// log: 45772 0xb2cc (pg:178, off:204)-->phy:  7884 (frm:  30) (prv: 241)--> val:    0 == value:    0 --  +    HIT!
// log: 27496 0x6b68 (pg:107, off:104)-->phy: 15720 (frm:  61) (prv: 241)--> val:    0 == value:    0 --  +    HIT!
// log: 46842 0xb6fa (pg:182, off:250)-->phy: 55546 (frm: 216) (prv: 241)--> val:   45 == value:   45 --  +    HIT!

// log: 38734 0x974e (pg:151, off: 78)-->phy: 46926 (frm: 183) (prv: 241)--> val:   37 == value:   37 --  +    HIT!
// log: 28972 0x712c (pg:113, off: 44)-->phy: 16428 (frm:  64) (prv: 241)--> val:    0 == value:    0 --  +    HIT!
// log: 59684 0xe924 (pg:233, off: 36)-->phy: 47396 (frm: 185) (prv: 241)--> val:    0 == value:    0 --  +    HIT!
// log: 11384 0x2c78 (pg: 44, off:120)-->phy: 54392 (frm: 212) (prv: 241)--> val:    0 == value:    0 --  +    HIT!
// log: 21018 0x521a (pg: 82, off: 26)-->phy: 37658 (frm: 147) (prv: 241)--> val:   20 == value:   20 --  +    HIT!

// log:  2192 0x0890 (pg:  8, off:144)-->phy: 35216 (frm: 137) (prv: 241)--> val:    0 == value:    0 --  +    HIT!
// log: 18384 0x47d0 (pg: 71, off:208)-->phy:  2512 (frm:   9) (prv: 241)--> val:    0 == value:    0 --  +    HIT!
// log: 13464 0x3498 (pg: 52, off:152)-->phy: 36248 (frm: 141) (prv: 241)--> val:    0 == value:    0 --  +    HIT!
// log: 31018 0x792a (pg:121, off: 42)-->phy: 61994 (frm: 242) (prv: 241)--> val:   30 == value:   30 --  + ----> pg_fault
// log: 62958 0xf5ee (pg:245, off:238)-->phy: 28654 (frm: 111) (prv: 242)--> val:   61 == value:   61 --  +    HIT!

// log: 30611 0x7793 (pg:119, off:147)-->phy: 16275 (frm:  63) (prv: 242)--> val:  -28 == value:  -28 --  +    HIT!
// log:  1913 0x0779 (pg:  7, off:121)-->phy: 33145 (frm: 129) (prv: 242)--> val:    0 == value:    0 --  +    HIT!
// log: 18904 0x49d8 (pg: 73, off:216)-->phy: 61912 (frm: 241) (prv: 242)--> val:    0 == value:    0 --  +    HIT!
// log: 26773 0x6895 (pg:104, off:149)-->phy: 42645 (frm: 166) (prv: 242)--> val:    0 == value:    0 --  +    HIT!
// log: 55491 0xd8c3 (pg:216, off:195)-->phy: 58307 (frm: 227) (prv: 242)--> val:   48 == value:   48 --  +    HIT!

// log: 21899 0x558b (pg: 85, off:139)-->phy: 24203 (frm:  94) (prv: 242)--> val:   98 == value:   98 --  +    HIT!
// log: 64413 0xfb9d (pg:251, off:157)-->phy:  5021 (frm:  19) (prv: 242)--> val:    0 == value:    0 --  +    HIT!
// log: 47134 0xb81e (pg:184, off: 30)-->phy: 29726 (frm: 116) (prv: 242)--> val:   46 == value:   46 --  +    HIT!
// log: 23172 0x5a84 (pg: 90, off:132)-->phy: 35972 (frm: 140) (prv: 242)--> val:    0 == value:    0 --  +    HIT!
// log:  7262 0x1c5e (pg: 28, off: 94)-->phy: 43102 (frm: 168) (prv: 242)--> val:    7 == value:    7 --  +    HIT!

// log: 12705 0x31a1 (pg: 49, off:161)-->phy: 31137 (frm: 121) (prv: 242)--> val:    0 == value:    0 --  +    HIT!
// log:  7522 0x1d62 (pg: 29, off: 98)-->phy:  6242 (frm:  24) (prv: 242)--> val:    7 == value:    7 --  +    HIT!
// log: 58815 0xe5bf (pg:229, off:191)-->phy: 55231 (frm: 215) (prv: 242)--> val:  111 == value:  111 --  +    HIT!
// log: 34916 0x8864 (pg:136, off:100)-->phy: 26724 (frm: 104) (prv: 242)--> val:    0 == value:    0 --  +    HIT!
// log:  3802 0x0eda (pg: 14, off:218)-->phy: 10202 (frm:  39) (prv: 242)--> val:    3 == value:    3 --  +    HIT!

// log: 58008 0xe298 (pg:226, off:152)-->phy:  3224 (frm:  12) (prv: 242)--> val:    0 == value:    0 --  +    HIT!
// log:  1239 0x04d7 (pg:  4, off:215)-->phy: 40919 (frm: 159) (prv: 242)--> val:   53 == value:   53 --  +    HIT!
// log: 63947 0xf9cb (pg:249, off:203)-->phy: 21963 (frm:  85) (prv: 242)--> val:  114 == value:  114 --  +    HIT!
// log:   381 0x017d (pg:  1, off:125)-->phy: 57213 (frm: 223) (prv: 242)--> val:    0 == value:    0 --  +    HIT!
// log: 60734 0xed3e (pg:237, off: 62)-->phy: 23614 (frm:  92) (prv: 242)--> val:   59 == value:   59 --  +    HIT!

// log: 48769 0xbe81 (pg:190, off:129)-->phy: 32897 (frm: 128) (prv: 242)--> val:    0 == value:    0 --  +    HIT!
// log: 41938 0xa3d2 (pg:163, off:210)-->phy: 38866 (frm: 151) (prv: 242)--> val:   40 == value:   40 --  +    HIT!
// log: 38025 0x9489 (pg:148, off:137)-->phy: 37257 (frm: 145) (prv: 242)--> val:    0 == value:    0 --  +    HIT!
// log: 55099 0xd73b (pg:215, off: 59)-->phy:  5179 (frm:  20) (prv: 242)--> val:  -50 == value:  -50 --  +    HIT!
// log: 56691 0xdd73 (pg:221, off:115)-->phy: 13427 (frm:  52) (prv: 242)--> val:   92 == value:   92 --  +    HIT!

// log: 39530 0x9a6a (pg:154, off:106)-->phy: 43370 (frm: 169) (prv: 242)--> val:   38 == value:   38 --  +    HIT!
// log: 59003 0xe67b (pg:230, off:123)-->phy: 14971 (frm:  58) (prv: 242)--> val:  -98 == value:  -98 --  +    HIT!
// log:  6029 0x178d (pg: 23, off:141)-->phy:  9357 (frm:  36) (prv: 242)--> val:    0 == value:    0 --  +    HIT!
// log: 20920 0x51b8 (pg: 81, off:184)-->phy: 39096 (frm: 152) (prv: 242)--> val:    0 == value:    0 --  +    HIT!
// log:  8077 0x1f8d (pg: 31, off:141)-->phy: 33933 (frm: 132) (prv: 242)--> val:    0 == value:    0 --  +    HIT!

// log: 42633 0xa689 (pg:166, off:137)-->phy: 20873 (frm:  81) (prv: 242)--> val:    0 == value:    0 --  +    HIT!
// log: 17443 0x4423 (pg: 68, off: 35)-->phy: 40995 (frm: 160) (prv: 242)--> val:    8 == value:    8 --  +    HIT!
// log: 53570 0xd142 (pg:209, off: 66)-->phy:   834 (frm:   3) (prv: 242)--> val:   52 == value:   52 --  +    HIT!
// log: 22833 0x5931 (pg: 89, off: 49)-->phy: 52785 (frm: 206) (prv: 242)--> val:    0 == value:    0 --  +    HIT!
// log:  3782 0x0ec6 (pg: 14, off:198)-->phy: 10182 (frm:  39) (prv: 242)--> val:    3 == value:    3 --  +    HIT!

// log: 47758 0xba8e (pg:186, off:142)-->phy: 56462 (frm: 220) (prv: 242)--> val:   46 == value:   46 --  +    HIT!
// log: 22136 0x5678 (pg: 86, off:120)-->phy: 56696 (frm: 221) (prv: 242)--> val:    0 == value:    0 --  +    HIT!
// log: 22427 0x579b (pg: 87, off:155)-->phy: 11419 (frm:  44) (prv: 242)--> val:  -26 == value:  -26 --  +    HIT!
// log: 23867 0x5d3b (pg: 93, off: 59)-->phy: 39483 (frm: 154) (prv: 242)--> val:   78 == value:   78 --  +    HIT!
// log: 59968 0xea40 (pg:234, off: 64)-->phy: 10560 (frm:  41) (prv: 242)--> val:    0 == value:    0 --  +    HIT!

// log: 62166 0xf2d6 (pg:242, off:214)-->phy: 62422 (frm: 243) (prv: 242)--> val:   60 == value:   60 --  + ----> pg_fault
// log:  6972 0x1b3c (pg: 27, off: 60)-->phy: 26428 (frm: 103) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 63684 0xf8c4 (pg:248, off:196)-->phy: 28356 (frm: 110) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 46388 0xb534 (pg:181, off: 52)-->phy: 50228 (frm: 196) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 41942 0xa3d6 (pg:163, off:214)-->phy: 38870 (frm: 151) (prv: 243)--> val:   40 == value:   40 --  +    HIT!

// log: 36524 0x8eac (pg:142, off:172)-->phy: 14508 (frm:  56) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log:  9323 0x246b (pg: 36, off:107)-->phy: 22891 (frm:  89) (prv: 243)--> val:   26 == value:   26 --  +    HIT!
// log: 31114 0x798a (pg:121, off:138)-->phy: 62090 (frm: 242) (prv: 243)--> val:   30 == value:   30 --  +    HIT!
// log: 22345 0x5749 (pg: 87, off: 73)-->phy: 11337 (frm:  44) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 46463 0xb57f (pg:181, off:127)-->phy: 50303 (frm: 196) (prv: 243)--> val:   95 == value:   95 --  +    HIT!

// log: 54671 0xd58f (pg:213, off:143)-->phy: 48783 (frm: 190) (prv: 243)--> val:   99 == value:   99 --  +    HIT!
// log:  9214 0x23fe (pg: 35, off:254)-->phy: 48382 (frm: 188) (prv: 243)--> val:    8 == value:    8 --  +    HIT!
// log:  7257 0x1c59 (pg: 28, off: 89)-->phy: 43097 (frm: 168) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 33150 0x817e (pg:129, off:126)-->phy: 53630 (frm: 209) (prv: 243)--> val:   32 == value:   32 --  +    HIT!
// log: 41565 0xa25d (pg:162, off: 93)-->phy: 41821 (frm: 163) (prv: 243)--> val:    0 == value:    0 --  +    HIT!

// log: 26214 0x6666 (pg:102, off:102)-->phy: 49254 (frm: 192) (prv: 243)--> val:   25 == value:   25 --  +    HIT!
// log:  3595 0x0e0b (pg: 14, off: 11)-->phy:  9995 (frm:  39) (prv: 243)--> val: -126 == value: -126 --  +    HIT!
// log: 17932 0x460c (pg: 70, off: 12)-->phy: 18700 (frm:  73) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 34660 0x8764 (pg:135, off:100)-->phy: 18020 (frm:  70) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 51961 0xcaf9 (pg:202, off:249)-->phy: 27385 (frm: 106) (prv: 243)--> val:    0 == value:    0 --  +    HIT!

// log: 58634 0xe50a (pg:229, off: 10)-->phy: 55050 (frm: 215) (prv: 243)--> val:   57 == value:   57 --  +    HIT!
// log: 57990 0xe286 (pg:226, off:134)-->phy:  3206 (frm:  12) (prv: 243)--> val:   56 == value:   56 --  +    HIT!
// log: 28848 0x70b0 (pg:112, off:176)-->phy:  1456 (frm:   5) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 49920 0xc300 (pg:195, off:  0)-->phy: 59136 (frm: 231) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 18351 0x47af (pg: 71, off:175)-->phy:  2479 (frm:   9) (prv: 243)--> val:  -21 == value:  -21 --  +    HIT!

// log: 53669 0xd1a5 (pg:209, off:165)-->phy:   933 (frm:   3) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 33996 0x84cc (pg:132, off:204)-->phy: 42188 (frm: 164) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log:  6741 0x1a55 (pg: 26, off: 85)-->phy:  6741 (frm:  26) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 64098 0xfa62 (pg:250, off: 98)-->phy:  4450 (frm:  17) (prv: 243)--> val:   62 == value:   62 --  +    HIT!
// log:   606 0x025e (pg:  2, off: 94)-->phy: 17246 (frm:  67) (prv: 243)--> val:    0 == value:    0 --  +    HIT!

// log: 27383 0x6af7 (pg:106, off:247)-->phy: 28919 (frm: 112) (prv: 243)--> val:  -67 == value:  -67 --  +    HIT!
// log: 63140 0xf6a4 (pg:246, off:164)-->phy: 22180 (frm:  86) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 32228 0x7de4 (pg:125, off:228)-->phy: 49892 (frm: 194) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 63437 0xf7cd (pg:247, off:205)-->phy: 17101 (frm:  66) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 29085 0x719d (pg:113, off:157)-->phy: 16541 (frm:  64) (prv: 243)--> val:    0 == value:    0 --  +    HIT!

// log: 65080 0xfe38 (pg:254, off: 56)-->phy: 48440 (frm: 189) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 38753 0x9761 (pg:151, off: 97)-->phy: 46945 (frm: 183) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 16041 0x3ea9 (pg: 62, off:169)-->phy: 21161 (frm:  82) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log:  9041 0x2351 (pg: 35, off: 81)-->phy: 48209 (frm: 188) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 42090 0xa46a (pg:164, off:106)-->phy: 46186 (frm: 180) (prv: 243)--> val:   41 == value:   41 --  +    HIT!

// log: 46388 0xb534 (pg:181, off: 52)-->phy: 50228 (frm: 196) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 63650 0xf8a2 (pg:248, off:162)-->phy: 28322 (frm: 110) (prv: 243)--> val:   62 == value:   62 --  +    HIT!
// log: 36636 0x8f1c (pg:143, off: 28)-->phy: 35612 (frm: 139) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 21947 0x55bb (pg: 85, off:187)-->phy: 24251 (frm:  94) (prv: 243)--> val:  110 == value:  110 --  +    HIT!
// log: 19833 0x4d79 (pg: 77, off:121)-->phy: 36473 (frm: 142) (prv: 243)--> val:    0 == value:    0 --  +    HIT!

// log: 36464 0x8e70 (pg:142, off:112)-->phy: 14448 (frm:  56) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log:  8541 0x215d (pg: 33, off: 93)-->phy: 20061 (frm:  78) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 12712 0x31a8 (pg: 49, off:168)-->phy: 31144 (frm: 121) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 48955 0xbf3b (pg:191, off: 59)-->phy: 50491 (frm: 197) (prv: 243)--> val:  -50 == value:  -50 --  +    HIT!
// log: 39206 0x9926 (pg:153, off: 38)-->phy: 55590 (frm: 217) (prv: 243)--> val:   38 == value:   38 --  +    HIT!

// log: 15578 0x3cda (pg: 60, off:218)-->phy: 31962 (frm: 124) (prv: 243)--> val:   15 == value:   15 --  +    HIT!
// log: 49205 0xc035 (pg:192, off: 53)-->phy:  8501 (frm:  33) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log:  7731 0x1e33 (pg: 30, off: 51)-->phy: 57907 (frm: 226) (prv: 243)--> val: -116 == value: -116 --  +    HIT!
// log: 43046 0xa826 (pg:168, off: 38)-->phy: 17446 (frm:  68) (prv: 243)--> val:   42 == value:   42 --  +    HIT!
// log: 60498 0xec52 (pg:236, off: 82)-->phy:  7250 (frm:  28) (prv: 243)--> val:   59 == value:   59 --  +    HIT!

// log:  9237 0x2415 (pg: 36, off: 21)-->phy: 22805 (frm:  89) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 47706 0xba5a (pg:186, off: 90)-->phy: 56410 (frm: 220) (prv: 243)--> val:   46 == value:   46 --  +    HIT!
// log: 43973 0xabc5 (pg:171, off:197)-->phy: 57541 (frm: 224) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 42008 0xa418 (pg:164, off: 24)-->phy: 46104 (frm: 180) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 27460 0x6b44 (pg:107, off: 68)-->phy: 15684 (frm:  61) (prv: 243)--> val:    0 == value:    0 --  +    HIT!

// log: 24999 0x61a7 (pg: 97, off:167)-->phy: 52647 (frm: 205) (prv: 243)--> val:  105 == value:  105 --  +    HIT!
// log: 51933 0xcadd (pg:202, off:221)-->phy: 27357 (frm: 106) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 34070 0x8516 (pg:133, off: 22)-->phy: 60950 (frm: 238) (prv: 243)--> val:   33 == value:   33 --  +    HIT!
// log: 65155 0xfe83 (pg:254, off:131)-->phy: 48515 (frm: 189) (prv: 243)--> val:  -96 == value:  -96 --  +    HIT!
// log: 59955 0xea33 (pg:234, off: 51)-->phy: 10547 (frm:  41) (prv: 243)--> val: -116 == value: -116 --  +    HIT!

// log:  9277 0x243d (pg: 36, off: 61)-->phy: 22845 (frm:  89) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 20420 0x4fc4 (pg: 79, off:196)-->phy: 16836 (frm:  65) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 44860 0xaf3c (pg:175, off: 60)-->phy: 13116 (frm:  51) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 50992 0xc730 (pg:199, off: 48)-->phy: 42800 (frm: 167) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 10583 0x2957 (pg: 41, off: 87)-->phy: 27479 (frm: 107) (prv: 243)--> val:   85 == value:   85 --  +    HIT!

// log: 57751 0xe197 (pg:225, off:151)-->phy: 61335 (frm: 239) (prv: 243)--> val:  101 == value:  101 --  +    HIT!
// log: 23195 0x5a9b (pg: 90, off:155)-->phy: 35995 (frm: 140) (prv: 243)--> val:  -90 == value:  -90 --  +    HIT!
// log: 27227 0x6a5b (pg:106, off: 91)-->phy: 28763 (frm: 112) (prv: 243)--> val: -106 == value: -106 --  +    HIT!
// log: 42816 0xa740 (pg:167, off: 64)-->phy: 19520 (frm:  76) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 58219 0xe36b (pg:227, off:107)-->phy: 34155 (frm: 133) (prv: 243)--> val:  -38 == value:  -38 --  +    HIT!

// log: 37606 0x92e6 (pg:146, off:230)-->phy: 21478 (frm:  83) (prv: 243)--> val:   36 == value:   36 --  +    HIT!
// log: 18426 0x47fa (pg: 71, off:250)-->phy:  2554 (frm:   9) (prv: 243)--> val:   17 == value:   17 --  +    HIT!
// log: 21238 0x52f6 (pg: 82, off:246)-->phy: 37878 (frm: 147) (prv: 243)--> val:   20 == value:   20 --  +    HIT!
// log: 11983 0x2ecf (pg: 46, off:207)-->phy: 59855 (frm: 233) (prv: 243)--> val:  -77 == value:  -77 --  +    HIT!
// log: 48394 0xbd0a (pg:189, off: 10)-->phy:  1802 (frm:   7) (prv: 243)--> val:   47 == value:   47 --  +    HIT!

// log: 11036 0x2b1c (pg: 43, off: 28)-->phy: 39964 (frm: 156) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 30557 0x775d (pg:119, off: 93)-->phy: 16221 (frm:  63) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 23453 0x5b9d (pg: 91, off:157)-->phy: 20637 (frm:  80) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 49847 0xc2b7 (pg:194, off:183)-->phy: 31671 (frm: 123) (prv: 243)--> val:  -83 == value:  -83 --  +    HIT!
// log: 30032 0x7550 (pg:117, off: 80)-->phy:   592 (frm:   2) (prv: 243)--> val:    0 == value:    0 --  +    HIT!

// log: 48065 0xbbc1 (pg:187, off:193)-->phy: 25793 (frm: 100) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log:  6957 0x1b2d (pg: 27, off: 45)-->phy: 26413 (frm: 103) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log:  2301 0x08fd (pg:  8, off:253)-->phy: 35325 (frm: 137) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log:  7736 0x1e38 (pg: 30, off: 56)-->phy: 57912 (frm: 226) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 31260 0x7a1c (pg:122, off: 28)-->phy: 23324 (frm:  91) (prv: 243)--> val:    0 == value:    0 --  +    HIT!

// log: 17071 0x42af (pg: 66, off:175)-->phy:   175 (frm:   0) (prv: 243)--> val:  -85 == value:  -85 --  +    HIT!
// log:  8940 0x22ec (pg: 34, off:236)-->phy: 46572 (frm: 181) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log:  9929 0x26c9 (pg: 38, off:201)-->phy: 44745 (frm: 174) (prv: 243)--> val:    0 == value:    0 --  +    HIT!
// log: 45563 0xb1fb (pg:177, off:251)-->phy: 46075 (frm: 179) (prv: 243)--> val:  126 == value:  126 --  +    HIT!
// log: 12107 0x2f4b (pg: 47, off: 75)-->phy:  2635 (frm:  10) (prv: 243)--> val:  -46 == value:  -46 --  +    HIT!


// Page Fault Percentage: 0.244%
// TLB Hit Percentage: 0.055%

// ALL logical ---> physical assertions PASSED!

// ---------------------------------------------------------------------------------------------------------------------------------

// Output to console: 128 page frames

// epw@EPWPC:~/mem_manager$ sudo g++ mem_mgr.cpp -o memmgr
// epw@EPWPC:~/mem_manager$ ./memmgr
// log: 16916 0x4214 (pg: 66, off: 20)-->phy:    20 (frm:   0) (prv:   0)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 62493 0xf41d (pg:244, off: 29)-->phy:   285 (frm:   1) (prv:   0)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 30198 0x75f6 (pg:117, off:246)-->phy:   758 (frm:   2) (prv:   1)--> val:   29 == value:   29 --  + ----> pg_fault
// log: 53683 0xd1b3 (pg:209, off:179)-->phy:   947 (frm:   3) (prv:   2)--> val:  108 == value:  108 --  + ----> pg_fault
// log: 40185 0x9cf9 (pg:156, off:249)-->phy:  1273 (frm:   4) (prv:   3)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 28781 0x706d (pg:112, off:109)-->phy:  1389 (frm:   5) (prv:   4)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 24462 0x5f8e (pg: 95, off:142)-->phy:  1678 (frm:   6) (prv:   5)--> val:   23 == value:   23 --  + ----> pg_fault
// log: 48399 0xbd0f (pg:189, off: 15)-->phy:  1807 (frm:   7) (prv:   6)--> val:   67 == value:   67 --  + ----> pg_fault
// log: 64815 0xfd2f (pg:253, off: 47)-->phy:  2095 (frm:   8) (prv:   7)--> val:   75 == value:   75 --  + ----> pg_fault
// log: 18295 0x4777 (pg: 71, off:119)-->phy:  2423 (frm:   9) (prv:   8)--> val:  -35 == value:  -35 --  + ----> pg_fault

// log: 12218 0x2fba (pg: 47, off:186)-->phy:  2746 (frm:  10) (prv:   9)--> val:   11 == value:   11 --  + ----> pg_fault
// log: 22760 0x58e8 (pg: 88, off:232)-->phy:  3048 (frm:  11) (prv:  10)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 57982 0xe27e (pg:226, off:126)-->phy:  3198 (frm:  12) (prv:  11)--> val:   56 == value:   56 --  + ----> pg_fault
// log: 27966 0x6d3e (pg:109, off: 62)-->phy:  3390 (frm:  13) (prv:  12)--> val:   27 == value:   27 --  + ----> pg_fault
// log: 54894 0xd66e (pg:214, off:110)-->phy:  3694 (frm:  14) (prv:  13)--> val:   53 == value:   53 --  + ----> pg_fault

// log: 38929 0x9811 (pg:152, off: 17)-->phy:  3857 (frm:  15) (prv:  14)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 32865 0x8061 (pg:128, off: 97)-->phy:  4193 (frm:  16) (prv:  15)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 64243 0xfaf3 (pg:250, off:243)-->phy:  4595 (frm:  17) (prv:  16)--> val:  -68 == value:  -68 --  + ----> pg_fault
// log:  2315 0x090b (pg:  9, off: 11)-->phy:  4619 (frm:  18) (prv:  17)--> val:   66 == value:   66 --  + ----> pg_fault
// log: 64454 0xfbc6 (pg:251, off:198)-->phy:  5062 (frm:  19) (prv:  18)--> val:   62 == value:   62 --  + ----> pg_fault

// log: 55041 0xd701 (pg:215, off:  1)-->phy:  5121 (frm:  20) (prv:  19)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 18633 0x48c9 (pg: 72, off:201)-->phy:  5577 (frm:  21) (prv:  20)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 14557 0x38dd (pg: 56, off:221)-->phy:  5853 (frm:  22) (prv:  21)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 61006 0xee4e (pg:238, off: 78)-->phy:  5966 (frm:  23) (prv:  22)--> val:   59 == value:   59 --  + ----> pg_fault
// log: 62615 0xf497 (pg:244, off:151)-->phy:   407 (frm:   1) (prv:  23)--> val:   37 == value:   37 --  +    HIT!

// log:  7591 0x1da7 (pg: 29, off:167)-->phy:  6311 (frm:  24) (prv:  23)--> val:  105 == value:  105 --  + ----> pg_fault
// log: 64747 0xfceb (pg:252, off:235)-->phy:  6635 (frm:  25) (prv:  24)--> val:   58 == value:   58 --  + ----> pg_fault
// log:  6727 0x1a47 (pg: 26, off: 71)-->phy:  6727 (frm:  26) (prv:  25)--> val: -111 == value: -111 --  + ----> pg_fault
// log: 32315 0x7e3b (pg:126, off: 59)-->phy:  6971 (frm:  27) (prv:  26)--> val: -114 == value: -114 --  + ----> pg_fault
// log: 60645 0xece5 (pg:236, off:229)-->phy:  7397 (frm:  28) (prv:  27)--> val:    0 == value:    0 --  + ----> pg_fault

// log:  6308 0x18a4 (pg: 24, off:164)-->phy:  7588 (frm:  29) (prv:  28)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 45688 0xb278 (pg:178, off:120)-->phy:  7800 (frm:  30) (prv:  29)--> val:    0 == value:    0 --  + ----> pg_fault
// log:   969 0x03c9 (pg:  3, off:201)-->phy:  8137 (frm:  31) (prv:  30)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 40891 0x9fbb (pg:159, off:187)-->phy:  8379 (frm:  32) (prv:  31)--> val:  -18 == value:  -18 --  + ----> pg_fault
// log: 49294 0xc08e (pg:192, off:142)-->phy:  8590 (frm:  33) (prv:  32)--> val:   48 == value:   48 --  + ----> pg_fault

// log: 41118 0xa09e (pg:160, off:158)-->phy:  8862 (frm:  34) (prv:  33)--> val:   40 == value:   40 --  + ----> pg_fault
// log: 21395 0x5393 (pg: 83, off:147)-->phy:  9107 (frm:  35) (prv:  34)--> val:  -28 == value:  -28 --  + ----> pg_fault
// log:  6091 0x17cb (pg: 23, off:203)-->phy:  9419 (frm:  36) (prv:  35)--> val:  -14 == value:  -14 --  + ----> pg_fault
// log: 32541 0x7f1d (pg:127, off: 29)-->phy:  9501 (frm:  37) (prv:  36)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 17665 0x4501 (pg: 69, off:  1)-->phy:  9729 (frm:  38) (prv:  37)--> val:    0 == value:    0 --  + ----> pg_fault

// log:  3784 0x0ec8 (pg: 14, off:200)-->phy: 10184 (frm:  39) (prv:  38)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 28718 0x702e (pg:112, off: 46)-->phy:  1326 (frm:   5) (prv:  39)--> val:   28 == value:   28 --  +    HIT!
// log: 59240 0xe768 (pg:231, off:104)-->phy: 10344 (frm:  40) (prv:  39)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 40178 0x9cf2 (pg:156, off:242)-->phy:  1266 (frm:   4) (prv:  40)--> val:   39 == value:   39 --  +    HIT!
// log: 60086 0xeab6 (pg:234, off:182)-->phy: 10678 (frm:  41) (prv:  40)--> val:   58 == value:   58 --  + ----> pg_fault

// log: 42252 0xa50c (pg:165, off: 12)-->phy: 10764 (frm:  42) (prv:  41)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 44770 0xaee2 (pg:174, off:226)-->phy: 11234 (frm:  43) (prv:  42)--> val:   43 == value:   43 --  + ----> pg_fault
// log: 22514 0x57f2 (pg: 87, off:242)-->phy: 11506 (frm:  44) (prv:  43)--> val:   21 == value:   21 --  + ----> pg_fault
// log:  3067 0x0bfb (pg: 11, off:251)-->phy: 11771 (frm:  45) (prv:  44)--> val:   -2 == value:   -2 --  + ----> pg_fault
// log: 15757 0x3d8d (pg: 61, off:141)-->phy: 11917 (frm:  46) (prv:  45)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 31649 0x7ba1 (pg:123, off:161)-->phy: 12193 (frm:  47) (prv:  46)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 10842 0x2a5a (pg: 42, off: 90)-->phy: 12378 (frm:  48) (prv:  47)--> val:   10 == value:   10 --  + ----> pg_fault
// log: 43765 0xaaf5 (pg:170, off:245)-->phy: 12789 (frm:  49) (prv:  48)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 33405 0x827d (pg:130, off:125)-->phy: 12925 (frm:  50) (prv:  49)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 44954 0xaf9a (pg:175, off:154)-->phy: 13210 (frm:  51) (prv:  50)--> val:   43 == value:   43 --  + ----> pg_fault

// log: 56657 0xdd51 (pg:221, off: 81)-->phy: 13393 (frm:  52) (prv:  51)--> val:    0 == value:    0 --  + ----> pg_fault
// log:  5003 0x138b (pg: 19, off:139)-->phy: 13707 (frm:  53) (prv:  52)--> val:  -30 == value:  -30 --  + ----> pg_fault
// log: 50227 0xc433 (pg:196, off: 51)-->phy: 13875 (frm:  54) (prv:  53)--> val:   12 == value:   12 --  + ----> pg_fault
// log: 19358 0x4b9e (pg: 75, off:158)-->phy: 14238 (frm:  55) (prv:  54)--> val:   18 == value:   18 --  + ----> pg_fault
// log: 36529 0x8eb1 (pg:142, off:177)-->phy: 14513 (frm:  56) (prv:  55)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 10392 0x2898 (pg: 40, off:152)-->phy: 14744 (frm:  57) (prv:  56)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 58882 0xe602 (pg:230, off:  2)-->phy: 14850 (frm:  58) (prv:  57)--> val:   57 == value:   57 --  + ----> pg_fault
// log:  5129 0x1409 (pg: 20, off:  9)-->phy: 15113 (frm:  59) (prv:  58)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 58554 0xe4ba (pg:228, off:186)-->phy: 15546 (frm:  60) (prv:  59)--> val:   57 == value:   57 --  + ----> pg_fault
// log: 58584 0xe4d8 (pg:228, off:216)-->phy: 15576 (frm:  60) (prv:  60)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 27444 0x6b34 (pg:107, off: 52)-->phy: 15668 (frm:  61) (prv:  60)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 58982 0xe666 (pg:230, off:102)-->phy: 14950 (frm:  58) (prv:  61)--> val:   57 == value:   57 --  +    HIT!
// log: 51476 0xc914 (pg:201, off: 20)-->phy: 15892 (frm:  62) (prv:  61)--> val:    0 == value:    0 --  + ----> pg_fault
// log:  6796 0x1a8c (pg: 26, off:140)-->phy:  6796 (frm:  26) (prv:  62)--> val:    0 == value:    0 --  +    HIT!
// log: 21311 0x533f (pg: 83, off: 63)-->phy:  9023 (frm:  35) (prv:  62)--> val:  -49 == value:  -49 --  +    HIT!

// log: 30705 0x77f1 (pg:119, off:241)-->phy: 16369 (frm:  63) (prv:  62)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 28964 0x7124 (pg:113, off: 36)-->phy: 16420 (frm:  64) (prv:  63)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 41003 0xa02b (pg:160, off: 43)-->phy:  8747 (frm:  34) (prv:  64)--> val:   10 == value:   10 --  +    HIT!
// log: 20259 0x4f23 (pg: 79, off: 35)-->phy: 16675 (frm:  65) (prv:  64)--> val:  -56 == value:  -56 --  + ----> pg_fault
// log: 57857 0xe201 (pg:226, off:  1)-->phy:  3073 (frm:  12) (prv:  65)--> val:    0 == value:    0 --  +    HIT!

// log: 63258 0xf71a (pg:247, off: 26)-->phy: 16922 (frm:  66) (prv:  65)--> val:   61 == value:   61 --  + ----> pg_fault
// log: 36374 0x8e16 (pg:142, off: 22)-->phy: 14358 (frm:  56) (prv:  66)--> val:   35 == value:   35 --  +    HIT!
// log:   692 0x02b4 (pg:  2, off:180)-->phy: 17332 (frm:  67) (prv:  66)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 43121 0xa871 (pg:168, off:113)-->phy: 17521 (frm:  68) (prv:  67)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 48128 0xbc00 (pg:188, off:  0)-->phy: 17664 (frm:  69) (prv:  68)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 34561 0x8701 (pg:135, off:  1)-->phy: 17921 (frm:  70) (prv:  69)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 49213 0xc03d (pg:192, off: 61)-->phy:  8509 (frm:  33) (prv:  70)--> val:    0 == value:    0 --  +    HIT!
// log: 36922 0x903a (pg:144, off: 58)-->phy: 18234 (frm:  71) (prv:  70)--> val:   36 == value:   36 --  + ----> pg_fault
// log: 59162 0xe71a (pg:231, off: 26)-->phy: 10266 (frm:  40) (prv:  71)--> val:   57 == value:   57 --  +    HIT!
// log: 50552 0xc578 (pg:197, off:120)-->phy: 18552 (frm:  72) (prv:  71)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 17866 0x45ca (pg: 69, off:202)-->phy:  9930 (frm:  38) (prv:  72)--> val:   17 == value:   17 --  +    HIT!
// log: 18145 0x46e1 (pg: 70, off:225)-->phy: 18913 (frm:  73) (prv:  72)--> val:    0 == value:    0 --  + ----> pg_fault
// log:  3884 0x0f2c (pg: 15, off: 44)-->phy: 18988 (frm:  74) (prv:  73)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 54388 0xd474 (pg:212, off:116)-->phy: 19316 (frm:  75) (prv:  74)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 42932 0xa7b4 (pg:167, off:180)-->phy: 19636 (frm:  76) (prv:  75)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 46919 0xb747 (pg:183, off: 71)-->phy: 19783 (frm:  77) (prv:  76)--> val:  -47 == value:  -47 --  + ----> pg_fault
// log: 58892 0xe60c (pg:230, off: 12)-->phy: 14860 (frm:  58) (prv:  77)--> val:    0 == value:    0 --  +    HIT!
// log:  8620 0x21ac (pg: 33, off:172)-->phy: 20140 (frm:  78) (prv:  77)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 38336 0x95c0 (pg:149, off:192)-->phy: 20416 (frm:  79) (prv:  78)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 64357 0xfb65 (pg:251, off:101)-->phy:  4965 (frm:  19) (prv:  79)--> val:    0 == value:    0 --  +    HIT!

// log: 23387 0x5b5b (pg: 91, off: 91)-->phy: 20571 (frm:  80) (prv:  79)--> val:  -42 == value:  -42 --  + ----> pg_fault
// log: 42632 0xa688 (pg:166, off:136)-->phy: 20872 (frm:  81) (prv:  80)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 15913 0x3e29 (pg: 62, off: 41)-->phy: 21033 (frm:  82) (prv:  81)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 15679 0x3d3f (pg: 61, off: 63)-->phy: 11839 (frm:  46) (prv:  82)--> val:   79 == value:   79 --  +    HIT!
// log: 22501 0x57e5 (pg: 87, off:229)-->phy: 11493 (frm:  44) (prv:  82)--> val:    0 == value:    0 --  +    HIT!

// log: 37540 0x92a4 (pg:146, off:164)-->phy: 21412 (frm:  83) (prv:  82)--> val:    0 == value:    0 --  + ----> pg_fault
// log:  5527 0x1597 (pg: 21, off:151)-->phy: 21655 (frm:  84) (prv:  83)--> val:  101 == value:  101 --  + ----> pg_fault
// log: 63921 0xf9b1 (pg:249, off:177)-->phy: 21937 (frm:  85) (prv:  84)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 62716 0xf4fc (pg:244, off:252)-->phy:   508 (frm:   1) (prv:  85)--> val:    0 == value:    0 --  +    HIT!
// log: 32874 0x806a (pg:128, off:106)-->phy:  4202 (frm:  16) (prv:  85)--> val:   32 == value:   32 --  +    HIT!

// log: 64390 0xfb86 (pg:251, off:134)-->phy:  4998 (frm:  19) (prv:  85)--> val:   62 == value:   62 --  +    HIT!
// log: 63101 0xf67d (pg:246, off:125)-->phy: 22141 (frm:  86) (prv:  85)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 61802 0xf16a (pg:241, off:106)-->phy: 22378 (frm:  87) (prv:  86)--> val:   60 == value:   60 --  + ----> pg_fault
// log: 19648 0x4cc0 (pg: 76, off:192)-->phy: 22720 (frm:  88) (prv:  87)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 29031 0x7167 (pg:113, off:103)-->phy: 16487 (frm:  64) (prv:  88)--> val:   89 == value:   89 --  +    HIT!

// log: 44981 0xafb5 (pg:175, off:181)-->phy: 13237 (frm:  51) (prv:  88)--> val:    0 == value:    0 --  +    HIT!
// log: 28092 0x6dbc (pg:109, off:188)-->phy:  3516 (frm:  13) (prv:  88)--> val:    0 == value:    0 --  +    HIT!
// log:  9448 0x24e8 (pg: 36, off:232)-->phy: 23016 (frm:  89) (prv:  88)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 44744 0xaec8 (pg:174, off:200)-->phy: 11208 (frm:  43) (prv:  89)--> val:    0 == value:    0 --  +    HIT!
// log: 61496 0xf038 (pg:240, off: 56)-->phy: 23096 (frm:  90) (prv:  89)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 31453 0x7add (pg:122, off:221)-->phy: 23517 (frm:  91) (prv:  90)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 60746 0xed4a (pg:237, off: 74)-->phy: 23626 (frm:  92) (prv:  91)--> val:   59 == value:   59 --  + ----> pg_fault
// log: 12199 0x2fa7 (pg: 47, off:167)-->phy:  2727 (frm:  10) (prv:  92)--> val:  -23 == value:  -23 --  +    HIT!
// log: 62255 0xf32f (pg:243, off: 47)-->phy: 23855 (frm:  93) (prv:  92)--> val:  -53 == value:  -53 --  + ----> pg_fault
// log: 21793 0x5521 (pg: 85, off: 33)-->phy: 24097 (frm:  94) (prv:  93)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 26544 0x67b0 (pg:103, off:176)-->phy: 24496 (frm:  95) (prv:  94)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 14964 0x3a74 (pg: 58, off:116)-->phy: 24692 (frm:  96) (prv:  95)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 41462 0xa1f6 (pg:161, off:246)-->phy: 25078 (frm:  97) (prv:  96)--> val:   40 == value:   40 --  + ----> pg_fault
// log: 56089 0xdb19 (pg:219, off: 25)-->phy: 25113 (frm:  98) (prv:  97)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 52038 0xcb46 (pg:203, off: 70)-->phy: 25414 (frm:  99) (prv:  98)--> val:   50 == value:   50 --  + ----> pg_fault

// log: 47982 0xbb6e (pg:187, off:110)-->phy: 25710 (frm: 100) (prv:  99)--> val:   46 == value:   46 --  + ----> pg_fault
// log: 59484 0xe85c (pg:232, off: 92)-->phy: 25948 (frm: 101) (prv: 100)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 50924 0xc6ec (pg:198, off:236)-->phy: 26348 (frm: 102) (prv: 101)--> val:    0 == value:    0 --  + ----> pg_fault
// log:  6942 0x1b1e (pg: 27, off: 30)-->phy: 26398 (frm: 103) (prv: 102)--> val:    6 == value:    6 --  + ----> pg_fault
// log: 34998 0x88b6 (pg:136, off:182)-->phy: 26806 (frm: 104) (prv: 103)--> val:   34 == value:   34 --  + ----> pg_fault

// log: 27069 0x69bd (pg:105, off:189)-->phy: 27069 (frm: 105) (prv: 104)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 51926 0xcad6 (pg:202, off:214)-->phy: 27350 (frm: 106) (prv: 105)--> val:   50 == value:   50 --  + ----> pg_fault
// log: 60645 0xece5 (pg:236, off:229)-->phy:  7397 (frm:  28) (prv: 106)--> val:    0 == value:    0 --  +    HIT!
// log: 43181 0xa8ad (pg:168, off:173)-->phy: 17581 (frm:  68) (prv: 106)--> val:    0 == value:    0 --  +    HIT!
// log: 10559 0x293f (pg: 41, off: 63)-->phy: 27455 (frm: 107) (prv: 106)--> val:   79 == value:   79 --  + ----> pg_fault

// log:  4664 0x1238 (pg: 18, off: 56)-->phy: 27704 (frm: 108) (prv: 107)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 28578 0x6fa2 (pg:111, off:162)-->phy: 28066 (frm: 109) (prv: 108)--> val:   27 == value:   27 --  + ----> pg_fault
// log: 59516 0xe87c (pg:232, off:124)-->phy: 25980 (frm: 101) (prv: 109)--> val:    0 == value:    0 --  +    HIT!
// log: 38912 0x9800 (pg:152, off:  0)-->phy:  3840 (frm:  15) (prv: 109)--> val:    0 == value:    0 --  +    HIT!
// log: 63562 0xf84a (pg:248, off: 74)-->phy: 28234 (frm: 110) (prv: 109)--> val:   62 == value:   62 --  + ----> pg_fault

// log: 64846 0xfd4e (pg:253, off: 78)-->phy:  2126 (frm:   8) (prv: 110)--> val:   63 == value:   63 --  +    HIT!
// log: 62938 0xf5da (pg:245, off:218)-->phy: 28634 (frm: 111) (prv: 110)--> val:   61 == value:   61 --  + ----> pg_fault
// log: 27194 0x6a3a (pg:106, off: 58)-->phy: 28730 (frm: 112) (prv: 111)--> val:   26 == value:   26 --  + ----> pg_fault
// log: 28804 0x7084 (pg:112, off:132)-->phy:  1412 (frm:   5) (prv: 112)--> val:    0 == value:    0 --  +    HIT!
// log: 61703 0xf107 (pg:241, off:  7)-->phy: 22279 (frm:  87) (prv: 112)--> val:   65 == value:   65 --  +    HIT!

// log: 10998 0x2af6 (pg: 42, off:246)-->phy: 12534 (frm:  48) (prv: 112)--> val:   10 == value:   10 --  +    HIT!
// log:  6596 0x19c4 (pg: 25, off:196)-->phy: 29124 (frm: 113) (prv: 112)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 37721 0x9359 (pg:147, off: 89)-->phy: 29273 (frm: 114) (prv: 113)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 43430 0xa9a6 (pg:169, off:166)-->phy: 29606 (frm: 115) (prv: 114)--> val:   42 == value:   42 --  + ----> pg_fault
// log: 22692 0x58a4 (pg: 88, off:164)-->phy:  2980 (frm:  11) (prv: 115)--> val:    0 == value:    0 --  +    HIT!

// log: 62971 0xf5fb (pg:245, off:251)-->phy: 28667 (frm: 111) (prv: 115)--> val:  126 == value:  126 --  +    HIT!
// log: 47125 0xb815 (pg:184, off: 21)-->phy: 29717 (frm: 116) (prv: 115)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 52521 0xcd29 (pg:205, off: 41)-->phy: 29993 (frm: 117) (prv: 116)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 34646 0x8756 (pg:135, off: 86)-->phy: 18006 (frm:  70) (prv: 117)--> val:   33 == value:   33 --  +    HIT!
// log: 32889 0x8079 (pg:128, off:121)-->phy:  4217 (frm:  16) (prv: 117)--> val:    0 == value:    0 --  +    HIT!

// log: 13055 0x32ff (pg: 50, off:255)-->phy: 30463 (frm: 118) (prv: 117)--> val:  -65 == value:  -65 --  + ----> pg_fault
// log: 65416 0xff88 (pg:255, off:136)-->phy: 30600 (frm: 119) (prv: 118)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 62869 0xf595 (pg:245, off:149)-->phy: 28565 (frm: 111) (prv: 119)--> val:    0 == value:    0 --  +    HIT!
// log: 57314 0xdfe2 (pg:223, off:226)-->phy: 30946 (frm: 120) (prv: 119)--> val:   55 == value:   55 --  + ----> pg_fault
// log: 12659 0x3173 (pg: 49, off:115)-->phy: 31091 (frm: 121) (prv: 120)--> val:   92 == value:   92 --  + ----> pg_fault

// log: 14052 0x36e4 (pg: 54, off:228)-->phy: 31460 (frm: 122) (prv: 121)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 32956 0x80bc (pg:128, off:188)-->phy:  4284 (frm:  16) (prv: 122)--> val:    0 == value:    0 --  +    HIT!
// log: 49273 0xc079 (pg:192, off:121)-->phy:  8569 (frm:  33) (prv: 122)--> val:    0 == value:    0 --  +    HIT!
// log: 50352 0xc4b0 (pg:196, off:176)-->phy: 14000 (frm:  54) (prv: 122)--> val:    0 == value:    0 --  +    HIT!
// log: 49737 0xc249 (pg:194, off: 73)-->phy: 31561 (frm: 123) (prv: 122)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 15555 0x3cc3 (pg: 60, off:195)-->phy: 31939 (frm: 124) (prv: 123)--> val:   48 == value:   48 --  + ----> pg_fault
// log: 47475 0xb973 (pg:185, off:115)-->phy: 32115 (frm: 125) (prv: 124)--> val:   92 == value:   92 --  + ----> pg_fault
// log: 15328 0x3be0 (pg: 59, off:224)-->phy: 32480 (frm: 126) (prv: 125)--> val:    0 == value:    0 --  + ----> pg_fault
// log: 34621 0x873d (pg:135, off: 61)-->phy: 17981 (frm:  70) (prv: 126)--> val:    0 == value:    0 --  +    HIT!
// log: 51365 0xc8a5 (pg:200, off:165)-->phy: 32677 (frm: 127) (prv: 126)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 32820 0x8034 (pg:128, off: 52)-->phy:  4148 (frm:  16) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 48855 0xbed7 (pg:190, off:215)-->phy:   215 (frm:   0) (prv: 127)--> val:  -75 == value:  -75 --  +    HIT!
// log: 12224 0x2fc0 (pg: 47, off:192)-->phy:  2752 (frm:  10) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log:  2035 0x07f3 (pg:  7, off:243)-->phy:   499 (frm:   1) (prv: 127)--> val:   -4 == value:   -4 --  +    HIT!
// log: 60539 0xec7b (pg:236, off:123)-->phy:  7291 (frm:  28) (prv: 127)--> val:   30 == value:   30 --  +    HIT!

// log: 14595 0x3903 (pg: 57, off:  3)-->phy:   515 (frm:   2) (prv: 127)--> val:   64 == value:   64 --  +    HIT!
// log: 13853 0x361d (pg: 54, off: 29)-->phy: 31261 (frm: 122) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 24143 0x5e4f (pg: 94, off: 79)-->phy:   847 (frm:   3) (prv: 127)--> val: -109 == value: -109 --  +    HIT!
// log: 15216 0x3b70 (pg: 59, off:112)-->phy: 32368 (frm: 126) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log:  8113 0x1fb1 (pg: 31, off:177)-->phy:  1201 (frm:   4) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log: 22640 0x5870 (pg: 88, off:112)-->phy:  2928 (frm:  11) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 32978 0x80d2 (pg:128, off:210)-->phy:  4306 (frm:  16) (prv: 127)--> val:   32 == value:   32 --  +    HIT!
// log: 39151 0x98ef (pg:152, off:239)-->phy:  4079 (frm:  15) (prv: 127)--> val:   59 == value:   59 --  +    HIT!
// log: 19520 0x4c40 (pg: 76, off: 64)-->phy: 22592 (frm:  88) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 58141 0xe31d (pg:227, off: 29)-->phy:  1309 (frm:   5) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log: 63959 0xf9d7 (pg:249, off:215)-->phy: 21975 (frm:  85) (prv: 127)--> val:  117 == value:  117 --  +    HIT!
// log: 53040 0xcf30 (pg:207, off: 48)-->phy:  1584 (frm:   6) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 55842 0xda22 (pg:218, off: 34)-->phy:  1826 (frm:   7) (prv: 127)--> val:   54 == value:   54 --  +    HIT!
// log:   585 0x0249 (pg:  2, off: 73)-->phy: 17225 (frm:  67) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 51229 0xc81d (pg:200, off: 29)-->phy: 32541 (frm: 127) (prv: 127)--> val:    0 == value:    0 --  + ----> pg_fault

// log: 64181 0xfab5 (pg:250, off:181)-->phy:  4533 (frm:  17) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 54879 0xd65f (pg:214, off: 95)-->phy:  3679 (frm:  14) (prv: 127)--> val: -105 == value: -105 --  +    HIT!
// log: 28210 0x6e32 (pg:110, off: 50)-->phy:  2098 (frm:   8) (prv: 127)--> val:   27 == value:   27 --  +    HIT!
// log: 10268 0x281c (pg: 40, off: 28)-->phy: 14620 (frm:  57) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 15395 0x3c23 (pg: 60, off: 35)-->phy: 31779 (frm: 124) (prv: 127)--> val:    8 == value:    8 --  +    HIT!

// log: 12884 0x3254 (pg: 50, off: 84)-->phy: 30292 (frm: 118) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log:  2149 0x0865 (pg:  8, off:101)-->phy:  2405 (frm:   9) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 53483 0xd0eb (pg:208, off:235)-->phy:  2795 (frm:  10) (prv: 127)--> val:   58 == value:   58 --  +    HIT!
// log: 59606 0xe8d6 (pg:232, off:214)-->phy: 26070 (frm: 101) (prv: 127)--> val:   58 == value:   58 --  +    HIT!
// log: 14981 0x3a85 (pg: 58, off:133)-->phy: 24709 (frm:  96) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log: 36672 0x8f40 (pg:143, off: 64)-->phy:  2880 (frm:  11) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 23197 0x5a9d (pg: 90, off:157)-->phy:  3229 (frm:  12) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 36518 0x8ea6 (pg:142, off:166)-->phy: 14502 (frm:  56) (prv: 127)--> val:   35 == value:   35 --  +    HIT!
// log: 13361 0x3431 (pg: 52, off: 49)-->phy:  3377 (frm:  13) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 19810 0x4d62 (pg: 77, off: 98)-->phy:  3682 (frm:  14) (prv: 127)--> val:   19 == value:   19 --  +    HIT!

// log: 25955 0x6563 (pg:101, off: 99)-->phy:  3939 (frm:  15) (prv: 127)--> val:   88 == value:   88 --  +    HIT!
// log: 62678 0xf4d6 (pg:244, off:214)-->phy:   470 (frm:   1) (prv: 127)--> val:    1 == value:   61 -- fail   HIT!
// log: 26021 0x65a5 (pg:101, off:165)-->phy:  4005 (frm:  15) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 29409 0x72e1 (pg:114, off:225)-->phy:  4321 (frm:  16) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 38111 0x94df (pg:148, off:223)-->phy:  4575 (frm:  17) (prv: 127)--> val:   55 == value:   55 --  +    HIT!

// log: 58573 0xe4cd (pg:228, off:205)-->phy: 15565 (frm:  60) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 56840 0xde08 (pg:222, off:  8)-->phy:  4616 (frm:  18) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 41306 0xa15a (pg:161, off: 90)-->phy: 24922 (frm:  97) (prv: 127)--> val:   40 == value:   40 --  +    HIT!
// log: 54426 0xd49a (pg:212, off:154)-->phy: 19354 (frm:  75) (prv: 127)--> val:   53 == value:   53 --  +    HIT!
// log:  3617 0x0e21 (pg: 14, off: 33)-->phy: 10017 (frm:  39) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log: 50652 0xc5dc (pg:197, off:220)-->phy: 18652 (frm:  72) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 41452 0xa1ec (pg:161, off:236)-->phy: 25068 (frm:  97) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 20241 0x4f11 (pg: 79, off: 17)-->phy: 16657 (frm:  65) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 31723 0x7beb (pg:123, off:235)-->phy: 12267 (frm:  47) (prv: 127)--> val:   -6 == value:   -6 --  +    HIT!
// log: 53747 0xd1f3 (pg:209, off:243)-->phy:  1011 (frm:   3) (prv: 127)--> val:  -68 == value:  124 -- fail   HIT!

// log: 28550 0x6f86 (pg:111, off:134)-->phy: 28038 (frm: 109) (prv: 127)--> val:   27 == value:   27 --  +    HIT!
// log: 23402 0x5b6a (pg: 91, off:106)-->phy: 20586 (frm:  80) (prv: 127)--> val:   22 == value:   22 --  +    HIT!
// log: 21205 0x52d5 (pg: 82, off:213)-->phy:  5077 (frm:  19) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 56181 0xdb75 (pg:219, off:117)-->phy: 25205 (frm:  98) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 57470 0xe07e (pg:224, off:126)-->phy:  5246 (frm:  20) (prv: 127)--> val:   56 == value:   56 --  +    HIT!

// log: 39933 0x9bfd (pg:155, off:253)-->phy:  5629 (frm:  21) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 34964 0x8894 (pg:136, off:148)-->phy: 26772 (frm: 104) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 24781 0x60cd (pg: 96, off:205)-->phy:  5837 (frm:  22) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 41747 0xa313 (pg:163, off: 19)-->phy:  5907 (frm:  23) (prv: 127)--> val:  -60 == value:  -60 --  +    HIT!
// log: 62564 0xf464 (pg:244, off:100)-->phy:   356 (frm:   1) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log: 58461 0xe45d (pg:228, off: 93)-->phy: 15453 (frm:  60) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 20858 0x517a (pg: 81, off:122)-->phy:  6266 (frm:  24) (prv: 127)--> val:   20 == value:   20 --  +    HIT!
// log: 49301 0xc095 (pg:192, off:149)-->phy:  8597 (frm:  33) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 40572 0x9e7c (pg:158, off:124)-->phy:  6524 (frm:  25) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 23840 0x5d20 (pg: 93, off: 32)-->phy:  6688 (frm:  26) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log: 35278 0x89ce (pg:137, off:206)-->phy:  7118 (frm:  27) (prv: 127)--> val:   34 == value:   34 --  +    HIT!
// log: 62905 0xf5b9 (pg:245, off:185)-->phy: 28601 (frm: 111) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 56650 0xdd4a (pg:221, off: 74)-->phy: 13386 (frm:  52) (prv: 127)--> val:   55 == value:   55 --  +    HIT!
// log: 11149 0x2b8d (pg: 43, off:141)-->phy:  7309 (frm:  28) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 38920 0x9808 (pg:152, off:  8)-->phy:  3848 (frm:  15) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log: 23430 0x5b86 (pg: 91, off:134)-->phy: 20614 (frm:  80) (prv: 127)--> val:   22 == value:   22 --  +    HIT!
// log: 57592 0xe0f8 (pg:224, off:248)-->phy:  5368 (frm:  20) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log:  3080 0x0c08 (pg: 12, off:  8)-->phy:  7432 (frm:  29) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log:  6677 0x1a15 (pg: 26, off: 21)-->phy:  6677 (frm:  26) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 50704 0xc610 (pg:198, off: 16)-->phy: 26128 (frm: 102) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log: 51883 0xcaab (pg:202, off:171)-->phy: 27307 (frm: 106) (prv: 127)--> val:  -86 == value:  -86 --  +    HIT!
// log: 62799 0xf54f (pg:245, off: 79)-->phy: 28495 (frm: 111) (prv: 127)--> val:   83 == value:   83 --  +    HIT!
// log: 20188 0x4edc (pg: 78, off:220)-->phy:  7900 (frm:  30) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log:  1245 0x04dd (pg:  4, off:221)-->phy:  8157 (frm:  31) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 12220 0x2fbc (pg: 47, off:188)-->phy:  2748 (frm:  10) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log: 17602 0x44c2 (pg: 68, off:194)-->phy:  8386 (frm:  32) (prv: 127)--> val:   17 == value:   17 --  +    HIT!
// log: 28609 0x6fc1 (pg:111, off:193)-->phy: 28097 (frm: 109) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 42694 0xa6c6 (pg:166, off:198)-->phy: 20934 (frm:  81) (prv: 127)--> val:   41 == value:   41 --  +    HIT!
// log: 29826 0x7482 (pg:116, off:130)-->phy:  8578 (frm:  33) (prv: 127)--> val:   29 == value:   29 --  +    HIT!
// log: 13827 0x3603 (pg: 54, off:  3)-->phy: 31235 (frm: 122) (prv: 127)--> val: -128 == value: -128 --  +    HIT!

// log: 27336 0x6ac8 (pg:106, off:200)-->phy: 28872 (frm: 112) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 53343 0xd05f (pg:208, off: 95)-->phy:  2655 (frm:  10) (prv: 127)--> val:   23 == value:   23 --  +    HIT!
// log: 11533 0x2d0d (pg: 45, off: 13)-->phy:  8717 (frm:  34) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 41713 0xa2f1 (pg:162, off:241)-->phy:  9201 (frm:  35) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 33890 0x8462 (pg:132, off: 98)-->phy:  9314 (frm:  36) (prv: 127)--> val:   33 == value:   33 --  +    HIT!

// log:  4894 0x131e (pg: 19, off: 30)-->phy: 13598 (frm:  53) (prv: 127)--> val:    4 == value:    4 --  +    HIT!
// log: 57599 0xe0ff (pg:224, off:255)-->phy:  5375 (frm:  20) (prv: 127)--> val:   63 == value:   63 --  +    HIT!
// log:  3870 0x0f1e (pg: 15, off: 30)-->phy: 18974 (frm:  74) (prv: 127)--> val:    3 == value:    3 --  +    HIT!
// log: 58622 0xe4fe (pg:228, off:254)-->phy: 15614 (frm:  60) (prv: 127)--> val:   57 == value:   57 --  +    HIT!
// log: 29780 0x7454 (pg:116, off: 84)-->phy:  8532 (frm:  33) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log: 62553 0xf459 (pg:244, off: 89)-->phy:   345 (frm:   1) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log:  2303 0x08ff (pg:  8, off:255)-->phy:  2559 (frm:   9) (prv: 127)--> val:   63 == value:   63 --  +    HIT!
// log: 51915 0xcacb (pg:202, off:203)-->phy: 27339 (frm: 106) (prv: 127)--> val:  -78 == value:  -78 --  +    HIT!
// log:  6251 0x186b (pg: 24, off:107)-->phy:  7531 (frm:  29) (prv: 127)--> val:   26 == value:   26 --  +    HIT!
// log: 38107 0x94db (pg:148, off:219)-->phy:  4571 (frm:  17) (prv: 127)--> val:   54 == value:   54 --  +    HIT!

// log: 59325 0xe7bd (pg:231, off:189)-->phy: 10429 (frm:  40) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 61295 0xef6f (pg:239, off:111)-->phy:  9583 (frm:  37) (prv: 127)--> val:  -37 == value:  -37 --  +    HIT!
// log: 26699 0x684b (pg:104, off: 75)-->phy:  9803 (frm:  38) (prv: 127)--> val:   18 == value:   18 --  +    HIT!
// log: 51188 0xc7f4 (pg:199, off:244)-->phy: 10228 (frm:  39) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 59519 0xe87f (pg:232, off:127)-->phy: 25983 (frm: 101) (prv: 127)--> val:   31 == value:   31 --  +    HIT!

// log:  7345 0x1cb1 (pg: 28, off:177)-->phy: 10417 (frm:  40) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 20325 0x4f65 (pg: 79, off:101)-->phy: 16741 (frm:  65) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 39633 0x9ad1 (pg:154, off:209)-->phy: 10705 (frm:  41) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log:  1562 0x061a (pg:  6, off: 26)-->phy: 10778 (frm:  42) (prv: 127)--> val:    1 == value:    1 --  +    HIT!
// log:  7580 0x1d9c (pg: 29, off:156)-->phy:  6300 (frm:  24) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log:  8170 0x1fea (pg: 31, off:234)-->phy:  1258 (frm:   4) (prv: 127)--> val:    7 == value:    7 --  +    HIT!
// log: 62256 0xf330 (pg:243, off: 48)-->phy: 23856 (frm:  93) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 35823 0x8bef (pg:139, off:239)-->phy: 11247 (frm:  43) (prv: 127)--> val:   -5 == value:   -5 --  +    HIT!
// log: 27790 0x6c8e (pg:108, off:142)-->phy: 11406 (frm:  44) (prv: 127)--> val:   27 == value:   27 --  +    HIT!
// log: 13191 0x3387 (pg: 51, off:135)-->phy: 11655 (frm:  45) (prv: 127)--> val:  -31 == value:  -31 --  +    HIT!

// log:  9772 0x262c (pg: 38, off: 44)-->phy: 11820 (frm:  46) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log:  7477 0x1d35 (pg: 29, off: 53)-->phy:  6197 (frm:  24) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 44455 0xada7 (pg:173, off:167)-->phy: 12199 (frm:  47) (prv: 127)--> val:  105 == value:  105 --  +    HIT!
// log: 59546 0xe89a (pg:232, off:154)-->phy: 26010 (frm: 101) (prv: 127)--> val:   58 == value:   58 --  +    HIT!
// log: 49347 0xc0c3 (pg:192, off:195)-->phy:  8643 (frm:  33) (prv: 127)--> val:   48 == value:   48 --  +    HIT!

// log: 36539 0x8ebb (pg:142, off:187)-->phy: 14523 (frm:  56) (prv: 127)--> val:  -82 == value:  -82 --  +    HIT!
// log: 12453 0x30a5 (pg: 48, off:165)-->phy: 12453 (frm:  48) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 49640 0xc1e8 (pg:193, off:232)-->phy: 12776 (frm:  49) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 28290 0x6e82 (pg:110, off:130)-->phy:  2178 (frm:   8) (prv: 127)--> val:   27 == value:   27 --  +    HIT!
// log: 44817 0xaf11 (pg:175, off: 17)-->phy: 13073 (frm:  51) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log:  8565 0x2175 (pg: 33, off:117)-->phy: 20085 (frm:  78) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 16399 0x400f (pg: 64, off: 15)-->phy: 12815 (frm:  50) (prv: 127)--> val:    3 == value:    3 --  +    HIT!
// log: 41934 0xa3ce (pg:163, off:206)-->phy:  6094 (frm:  23) (prv: 127)--> val:   40 == value:   40 --  +    HIT!
// log: 45457 0xb191 (pg:177, off:145)-->phy: 13201 (frm:  51) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 33856 0x8440 (pg:132, off: 64)-->phy:  9280 (frm:  36) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log: 19498 0x4c2a (pg: 76, off: 42)-->phy: 22570 (frm:  88) (prv: 127)--> val:   19 == value:   19 --  +    HIT!
// log: 17661 0x44fd (pg: 68, off:253)-->phy:  8445 (frm:  32) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 63829 0xf955 (pg:249, off: 85)-->phy: 21845 (frm:  85) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 42034 0xa432 (pg:164, off: 50)-->phy: 13362 (frm:  52) (prv: 127)--> val:   41 == value:   41 --  +    HIT!
// log: 28928 0x7100 (pg:113, off:  0)-->phy: 16384 (frm:  64) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log: 30711 0x77f7 (pg:119, off:247)-->phy: 16375 (frm:  63) (prv: 127)--> val:   -3 == value:   -3 --  +    HIT!
// log:  8800 0x2260 (pg: 34, off: 96)-->phy: 13664 (frm:  53) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 52335 0xcc6f (pg:204, off:111)-->phy: 13935 (frm:  54) (prv: 127)--> val:   27 == value:   27 --  +    HIT!
// log: 38775 0x9777 (pg:151, off:119)-->phy: 14199 (frm:  55) (prv: 127)--> val:  -35 == value:  -35 --  +    HIT!
// log: 52704 0xcde0 (pg:205, off:224)-->phy: 30176 (frm: 117) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log: 24380 0x5f3c (pg: 95, off: 60)-->phy:  1596 (frm:   6) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 19602 0x4c92 (pg: 76, off:146)-->phy: 22674 (frm:  88) (prv: 127)--> val:   19 == value:   19 --  +    HIT!
// log: 57998 0xe28e (pg:226, off:142)-->phy:  3214 (frm:  12) (prv: 127)--> val:   22 == value:   56 -- fail   HIT!
// log:  2919 0x0b67 (pg: 11, off:103)-->phy: 11623 (frm:  45) (prv: 127)--> val:  -39 == value:  -39 --  +    HIT!
// log:  8362 0x20aa (pg: 32, off:170)-->phy: 14506 (frm:  56) (prv: 127)--> val:    8 == value:    8 --  +    HIT!

// log: 17884 0x45dc (pg: 69, off:220)-->phy:  9948 (frm:  38) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 45737 0xb2a9 (pg:178, off:169)-->phy:  7849 (frm:  30) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 47894 0xbb16 (pg:187, off: 22)-->phy: 25622 (frm: 100) (prv: 127)--> val:   46 == value:   46 --  +    HIT!
// log: 59667 0xe913 (pg:233, off: 19)-->phy: 14611 (frm:  57) (prv: 127)--> val:   68 == value:   68 --  +    HIT!
// log: 10385 0x2891 (pg: 40, off:145)-->phy: 14737 (frm:  57) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log: 52782 0xce2e (pg:206, off: 46)-->phy: 14894 (frm:  58) (prv: 127)--> val:   51 == value:   51 --  +    HIT!
// log: 64416 0xfba0 (pg:251, off:160)-->phy:  5024 (frm:  19) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 40946 0x9ff2 (pg:159, off:242)-->phy:  8434 (frm:  32) (prv: 127)--> val:   17 == value:   39 -- fail   HIT!
// log: 16778 0x418a (pg: 65, off:138)-->phy: 15242 (frm:  59) (prv: 127)--> val:   16 == value:   16 --  +    HIT!
// log: 27159 0x6a17 (pg:106, off: 23)-->phy: 28695 (frm: 112) (prv: 127)--> val: -123 == value: -123 --  +    HIT!

// log: 24324 0x5f04 (pg: 95, off:  4)-->phy:  1540 (frm:   6) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 32450 0x7ec2 (pg:126, off:194)-->phy:  7106 (frm:  27) (prv: 127)--> val:   34 == value:   31 -- fail   HIT!
// log:  9108 0x2394 (pg: 35, off:148)-->phy: 15508 (frm:  60) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 65305 0xff19 (pg:255, off: 25)-->phy: 30489 (frm: 119) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 19575 0x4c77 (pg: 76, off:119)-->phy: 22647 (frm:  88) (prv: 127)--> val:   29 == value:   29 --  +    HIT!

// log: 11117 0x2b6d (pg: 43, off:109)-->phy:  7277 (frm:  28) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 65170 0xfe92 (pg:254, off:146)-->phy: 15762 (frm:  61) (prv: 127)--> val:   63 == value:   63 --  +    HIT!
// log: 58013 0xe29d (pg:226, off:157)-->phy:  3229 (frm:  12) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 61676 0xf0ec (pg:240, off:236)-->phy: 23276 (frm:  90) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 63510 0xf816 (pg:248, off: 22)-->phy: 28182 (frm: 110) (prv: 127)--> val:   62 == value:   62 --  +    HIT!

// log: 17458 0x4432 (pg: 68, off: 50)-->phy:  8242 (frm:  32) (prv: 127)--> val:   17 == value:   17 --  +    HIT!
// log: 54675 0xd593 (pg:213, off:147)-->phy: 16019 (frm:  62) (prv: 127)--> val:  100 == value:  100 --  +    HIT!
// log:  1713 0x06b1 (pg:  6, off:177)-->phy: 10929 (frm:  42) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 55105 0xd741 (pg:215, off: 65)-->phy:  5185 (frm:  20) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 65321 0xff29 (pg:255, off: 41)-->phy: 30505 (frm: 119) (prv: 127)--> val:    0 == value:    0 --  +    HIT!

// log: 45278 0xb0de (pg:176, off:222)-->phy: 16350 (frm:  63) (prv: 127)--> val:   44 == value:   44 --  +    HIT!
// log: 26256 0x6690 (pg:102, off:144)-->phy: 16528 (frm:  64) (prv: 127)--> val:    0 == value:    0 --  +    HIT!
// log: 64198 0xfac6 (pg:250, off:198)-->phy:  4550 (frm:  17) (prv: 127)--> val:   37 == value:   62 -- fail   HIT!
// epw@EPWPC:~/mem_manager$ 