#include "z80.h"
#include <iostream>
#include "TMS5501.h"
#include "8251Uart.h"
#include <vector>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

using z80::fast_u8;
using z80::fast_u16;
using z80::least_u8;


#define SWITCH_LED 0xFFU
I8251Uart THE_UART(0x2);
TMS5501 TMSCHA(0x10, true);
TMS5501 TMSCHB(0x20);

//#define DEBUG

#define TOTAL_MEM   40 * 1024

#ifdef DEBUG
#define DEBUG_MEM_ACC
#endif

class IMSAIEmulator : public z80::z80_cpu<IMSAIEmulator> {
public:
  uint8_t memory[64 * 1024]{};
  size_t code_end;
  uint64_t cycle = 0;

  fast_u16 touched_a = 0, touched_bc = 0, touched_de = 0, touched_hl = 0, touched_ix = 0, touched_iy = 0;

  std::vector<fast_u16> shadow_call_stack;

  typedef z80::z80_cpu<IMSAIEmulator> base;

  IMSAIEmulator() = default;

  void dump_reg_info();

  void dump_stack_top();

  void on_set_pc(z80::fast_u16 pc) override {
    base::on_set_pc(pc);
  }

  void on_tick(unsigned t) {
    base ::on_tick(t);
    cycle += t;
  }

  fast_u8 on_read(fast_u16 addr) override;

  void on_write(fast_u16 addr, fast_u8 n) override;

  fast_u8 on_input(fast_u16 port) override;

  void on_output(fast_u16 port, fast_u8 n) override;

  void on_set_reg(z80::reg r, z80::iregp irp, fast_u8 d, fast_u8 n) override;

  void on_set_regp(z80::regp rp, fast_u16 nn) override;

  void on_set_iregp(fast_u16 nn) override;

  void on_call(z80::fast_u16 pc) {

//    if (pc == 0x120) {
//      std::printf("Call to memset... at %04lx\n", get_pc());
//      dump_stack_top();
//    }

    shadow_call_stack.push_back(get_pc());
    base::on_call(pc);
  }

  void on_ret() {
    auto addr = on_pop();
    on_push(addr);

    if (shadow_call_stack.empty()) {
      std::printf("empty shadow call stack.... \n");
      exit(1);
    } else {
      auto expected_addr = *(--shadow_call_stack.end());
      if (expected_addr != addr) {
        std::printf("Expected return to %04lx but got return to %04lx\n", expected_addr, addr);
      }
      shadow_call_stack.pop_back();
    }

    base::on_ret();
  }

};

void IMSAIEmulator::dump_reg_info() {
  std::printf("Regs:\n");
  std::printf("    BC=0x%04lx written at PC=0x%04lx\n", get_bc(), touched_bc);
  std::printf("    DE=0x%04lx written at PC=0x%04lx\n", get_de(), touched_de);
  std::printf("    HL=0x%04lx written at PC=0x%04lx\n", get_hl(), touched_hl);
  std::printf("    IX=0x%04lx written at PC=0x%04lx\n", get_ix(), touched_ix);
  std::printf("    IY=0x%04lx written at PC=0x%04lx\n", get_iy(), touched_iy);
}

void IMSAIEmulator::dump_stack_top() {
  int64_t start = (int64_t)get_sp() - 32;
  int64_t end = (int64_t)get_sp() + 11;
  if (start < 0)
    start = 0;
  if (end > 0xfffe)
    end = 0xfffe;

  std::printf("stack (increasing address):\n");
  for (int64_t addr = start; addr <= end; addr += 2) {
    if ((fast_u16)addr == get_sp())
      std::printf("sp->");
    else
      std::printf("    ");

    std::printf("%02x%02x\n", on_read((fast_u16)addr + 1), on_read((fast_u16)addr));
  }

}

void IMSAIEmulator::on_write(fast_u16 addr, fast_u8 n) {
  assert(addr < z80::address_space_size);
#ifdef DEBUG_MEM_ACC
  std::printf("write 0x%02x at 0x%04x\n", static_cast<unsigned>(n),
                static_cast<unsigned>(addr));
#endif
  if (addr >= TOTAL_MEM) {
    std::printf("Large mem write at 0x%04lx at PC=0x%04lx, SP=0x%04lx\n", addr, get_pc(), get_sp());
    return;
  }
  if (addr < code_end) {
    dump_reg_info();
    dump_stack_top();

    int x = (int)get_pc() - 10;
    if (x < 0)
      x = 0;

    for (fast_u16 a = (uint16_t)x; a <= get_pc(); a++)
      std::printf("%02x ", on_read(a));
    std::printf("\n");

    std::printf("Cycle: %ld: write to binary section at 0x%04lx at PC=0x%04lx, SP=0x%04lx\n", this->cycle, addr, get_pc(), get_sp());
    exit(1);
  }
  memory[addr] = static_cast<least_u8>(n);
}

fast_u8 IMSAIEmulator::on_read(fast_u16 addr) {
  assert(addr < z80::address_space_size);
  if (addr >= TOTAL_MEM)
    return 0xFF;
  fast_u8 n = memory[addr];
#ifdef DEBUG_MEM_ACC
  std::printf("read 0x%02x at 0x%04x\n", static_cast<unsigned>(n),
                static_cast<unsigned>(addr));
#endif
  return n;
}

void IMSAIEmulator::on_output(fast_u16 port, fast_u8 n) {
  switch (port & 0xFFU) {
    case SWITCH_LED:
      std::printf("Output LED: 0x%02x [0x%02x, %u]\n", n, (unsigned char) ~n, (unsigned char) ~n);
      break;
    default:
      THE_UART.doOut(static_cast<uint8_t>(port), static_cast<uint8_t>(n));
      TMSCHA.doOut(static_cast<uint8_t>(port), static_cast<uint8_t>(n));
      break;
  }
}

fast_u8 IMSAIEmulator::on_input(fast_u16 port) {
  int value = 0;
  switch (port & 0xFFU) {
    case SWITCH_LED:
      std::printf("Reading from SWITCH: ");
      std::cin >> value;
      std::printf("Parsed as: h:0x%02x, s:%d, u:%u\n", (unsigned char) value, (char) value, (unsigned char) value);
      return (unsigned char) value;
    default:
      fast_u8 val = 0;
      val |= THE_UART.doIn(static_cast<uint8_t>(port));
      val |= TMSCHA.doIn(static_cast<uint8_t>(port));
      return val;
  }
}

void IMSAIEmulator::on_set_reg(z80::reg r, z80::iregp irp, fast_u8 d, fast_u8 n)  {
  using namespace z80;
  switch (r) {
    case reg::b:
      touched_bc = get_pc();
      return self().on_set_b(n);
    case reg::c:
      touched_bc = get_pc();
      return self().on_set_c(n);
    case reg::d:
      touched_de = get_pc();
      return self().on_set_d(n);
    case reg::e:
      touched_de = get_pc();
      return self().on_set_e(n);
    case reg::at_hl:
      return write_at_disp(d, n);
    case reg::a:
      touched_a = get_pc();
      return self().on_set_a(n);
    case reg::h:
      switch (irp) {
        case iregp::hl:
          touched_hl = get_pc();
          return self().on_set_h(n);
        case iregp::ix:
          touched_ix = get_pc();
          return self().on_set_ixh(n);
        case iregp::iy:
          touched_iy = get_pc();
          return self().on_set_iyh(n);
      }
      break;
    case reg::l:
      switch (irp) {
        case iregp::hl:
          touched_hl = get_pc();
          return self().on_set_l(n);
        case iregp::ix:
          touched_ix = get_pc();
          return self().on_set_ixl(n);
        case iregp::iy:
          touched_iy = get_pc();
          return self().on_set_iyl(n);
      }
      break;
  }
  unreachable("Unknown register.");
}

void IMSAIEmulator::on_set_regp(z80::regp rp, fast_u16 nn) {
  using namespace z80;
  switch (rp) {
    case regp::bc:
      touched_bc = get_pc();
      return self().on_set_bc(nn);
    case regp::de:
      touched_de = get_pc();
      return self().on_set_de(nn);
    case regp::hl:
      // do not update touched_hl, it will be updated in on_set_iregp
      return self().on_set_iregp(nn);
    case regp::sp:
      return self().on_set_sp(nn);
  }
  unreachable("Unknown register.");
}

void IMSAIEmulator::on_set_iregp(fast_u16 nn) {
  using namespace z80;
  switch (self().on_get_iregp_kind()) {
    case iregp::hl:
      touched_hl = get_pc();
      return self().on_set_hl(nn);
    case iregp::ix:
      touched_ix = get_pc();
      return self().on_set_ix(nn);
    case iregp::iy:
      touched_iy = get_pc();
      return self().on_set_iy(nn);
  }
  unreachable("Unknown index register.");
}

struct termios orig_termios;
void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  struct termios raw = orig_termios;

  raw.c_iflag &= static_cast<unsigned int>(~(ICRNL));
  raw.c_lflag &= static_cast<unsigned int>(~(ECHO | ICANON));
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

extern "C" void cleanup(int) {
  exit(0);
}

extern "C" void beforeExit(void) {
  puts("Dying...\n");
  disableRawMode();
}

int main(int argc, char **argv) {
  atexit(beforeExit);
  signal(SIGINT, reinterpret_cast<__sighandler_t>(cleanup));

  enableRawMode();

//  char c;
//  ssize_t readval = 0;
//  while (readval = read(STDIN_FILENO, &c, 1), c != 'q') {
//    if (!readval)
//      continue;
//    if (iscntrl(c)) {
//      printf("%d\n", c);
//    } else {
//      printf("%d ('%c')\n", c, c);
//    }
//  }


  if (argc < 2) {
    fprintf(stderr, "No file provided. \n%s [file]", argv[0]);
    return 1;
  }

  IMSAIEmulator e;

  char *filename = argv[1];
  FILE *file = fopen(filename, "rb");

  size_t read = fread(e.memory, sizeof(uint8_t), 64 * 1024, file);
  std::printf("Loaded %zu bytes of memory\n", read);

  e.code_end = read;

  fclose(file);

  FILE *pc_file = fopen("pc_list.txt", "w");

  for (e.cycle = 0;;) {
    e.on_step();

    if (e.get_pc() > read) {
      std::fprintf(stderr, "PC has ran off at @ 0x%04lx", e.get_pc());
      exit(-1);
    }
    fprintf(pc_file, "%lu\n", (uint64_t) e.get_pc());

    fflush(pc_file);

    if (e.is_halted()) {
      std::printf("Halted after %zu cycles\n", e.cycle);
      break;
    }
  }

  fclose(pc_file);

}
