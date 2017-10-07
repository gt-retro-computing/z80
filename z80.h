
/*  Z80 CPU Simulator.

    Copyright (C) 2017 Ivan Kosarev.
    ivan@kosarev.info

    Published under the MIT license.
*/

#ifndef Z80_H
#define Z80_H

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace z80 {

typedef uint_fast8_t fast_u8;
typedef uint_least8_t least_u8;

typedef uint_fast16_t fast_u16;
typedef uint_least16_t least_u16;

typedef uint_fast32_t size_type;

static const fast_u8 mask8 = 0xff;
static const fast_u8 sign8_mask = 0x80;
static const fast_u16 mask16 = 0xffff;

static inline void unused(...) {}

static inline bool get_sign8(fast_u8 n) {
    return (n & sign8_mask) != 0;
}

static inline bool neg8(fast_u8 n) {
    return ((n ^ mask8) + 1) & mask8;
}

static inline bool abs8(fast_u8 n) {
    return !get_sign8(n) ? n : neg8(n);
}

static inline fast_u8 get_low8(fast_u16 n) {
    return static_cast<fast_u8>(n & mask8);
}

static inline fast_u8 get_high8(fast_u16 n) {
    return static_cast<fast_u8>((n >> 8) & mask8);
}

static inline fast_u16 make16(fast_u8 hi, fast_u8 lo) {
    return (static_cast<fast_u16>(hi) << 8) | lo;
}

static inline fast_u16 add16(fast_u16 a, fast_u16 b) {
    return (a + b) & mask16;
}

static inline fast_u16 sub16(fast_u16 a, fast_u16 b) {
    return (a - b) & mask16;
}

static inline fast_u16 inc16(fast_u16 n) {
    return add16(n, 1);
}

enum class reg { b, c, d, e, h, l, at_hl, a };

enum class index_reg { hl, ix, iy };

enum class alu { add, adc, sub, sbc, and_a, xor_a, or_a, cp };

template<typename D>
class instructions_decoder {
public:
    instructions_decoder() {}

    index_reg get_index_reg() const { return index_r; }

    fast_u8 fetch_disp_or_null(reg r = reg::at_hl) {
        if(get_index_reg() == index_reg::hl || r != reg::at_hl)
            return 0;
        fast_u8 d = (*this)->on_fetch_cycle();
        (*this)->on_exec5_cycle((*this)->get_last_fetch_addr());
        return d;
    }

    void on_decode() {
        fast_u8 op = (*this)->on_fetch_cycle();
        fast_u8 y = get_y_part(op);
        fast_u8 z = get_z_part(op);

        switch(op & x_mask) {
        case 0200: { // alu[y] r[z]
                     // alu r            f(4)
                     // alu (HL)         f(4)           r(3)
                     // alu (i+d)   f(4) f(4) r(3) e(5) r(3)
                     auto r = static_cast<reg>(z);
                     auto k = static_cast<alu>(y);
                     return (*this)->on_alu_r(k, r, fetch_disp_or_null(r)); }
        }
        switch(op) {
        case 0x00: return (*this)->on_nop();
        case 0xf3: return (*this)->on_di();
        }

        // TODO
        std::fprintf(stderr, "Unknown opcode 0x%02x at 0x%04x.\n",
                     static_cast<unsigned>(op),
                     static_cast<unsigned>((*this)->get_last_fetch_addr()));
        std::abort();
    }

    void decode() { (*this)->on_decode(); }

protected:
    D *operator -> () { return static_cast<D*>(this); }

    static const fast_u8 x_mask = 0300;

    static const fast_u8 y_mask = 0070;
    fast_u8 get_y_part(fast_u8 op) { return (op & y_mask) >> 3; }

    static const fast_u8 z_mask = 0007;
    fast_u8 get_z_part(fast_u8 op) { return op & z_mask; }

private:
    index_reg index_r;
};

class disassembler_base {
public:
    disassembler_base() {}
    virtual ~disassembler_base() {}

    virtual void on_output(const char *out) = 0;

    virtual void on_format(const char *fmt, ...);
};

template<typename D>
class disassembler : public disassembler_base {
public:
    disassembler() {}

    void on_exec5_cycle(fast_u16 addr) { unused(addr); }

    void on_alu_r(alu k, reg r, fast_u8 d) {
        (*this)->on_format("AR", k, static_cast<int>(r),
                           (*this)->get_index_reg(), d); }
    void on_di() { (*this)->on_format("di"); }
    void on_nop() { (*this)->on_format("nop"); }

    void disassemble() { (*this)->decode(); }

protected:
    D *operator -> () { return static_cast<D*>(this); }
};

class processor_base {
public:
    processor_base() {}

protected:
    static const unsigned sf_bit = 7;
    static const unsigned zf_bit = 6;
    static const unsigned yf_bit = 5;
    static const unsigned hf_bit = 4;
    static const unsigned xf_bit = 3;
    static const unsigned pf_bit = 2;
    static const unsigned nf_bit = 1;
    static const unsigned cf_bit = 0;

    static const fast_u8 sf_mask = 1 << sf_bit;
    static const fast_u8 zf_mask = 1 << zf_bit;
    static const fast_u8 yf_mask = 1 << yf_bit;
    static const fast_u8 hf_mask = 1 << hf_bit;
    static const fast_u8 xf_mask = 1 << xf_bit;
    static const fast_u8 pf_mask = 1 << pf_bit;
    static const fast_u8 nf_mask = 1 << nf_bit;
    static const fast_u8 cf_mask = 1 << cf_bit;

    fast_u8 zf_ari(fast_u8 n) {
        return static_cast<fast_u8>((n == 0 ? 1 : 0) << zf_bit);
    }

    bool pf_log4(fast_u8 n) {
        return 0x9669 & (1 << (n & 0xf));
    }

    fast_u8 pf_log(fast_u8 n) {
        bool lo = pf_log4(n);
        bool hi = pf_log4(n >> 4);
        return static_cast<fast_u8>((lo == hi ? 1 : 0) << pf_bit);
    }

    struct processor_state {
        processor_state()
            : last_fetch_addr(0),
              bc(0), de(0), hl(0), af(0),
              ix(0), iy(0),
              pc(0), memptr(0),
              iff1(false), iff2(false)
        {}

        fast_u16 last_fetch_addr;
        fast_u16 bc, de, hl, af;
        fast_u16 ix, iy;
        fast_u16 pc, memptr;
        bool iff1, iff2;
    } state;
};

template<typename D>
class processor : public processor_base {
public:
    processor() {}

    fast_u8 get_b() const { return get_high8(state.bc); }
    void set_b(fast_u8 b) { state.bc = make16(b, get_c()); }

    fast_u8 on_get_b() const { return get_b(); }
    void on_set_b(fast_u8 b) { set_b(b); }

    fast_u8 get_c() const { return get_low8(state.bc); }
    void set_c(fast_u8 c) { state.bc = make16(get_b(), c); }

    fast_u8 on_get_c() const { return get_c(); }
    void on_set_c(fast_u8 c) const { set_c(c); }

    fast_u8 get_d() const { return get_high8(state.de); }
    void set_d(fast_u8 d) { state.de = make16(d, get_e()); }

    fast_u8 on_get_d() const { return get_d(); }
    void on_set_d(fast_u8 d) { set_d(d); }

    fast_u8 get_e() const { return get_low8(state.de); }
    void set_e(fast_u8 e) { state.de = make16(get_d(), e); }

    fast_u8 on_get_e() const { return get_e(); }
    void on_set_e(fast_u8 e) const { set_e(e); }

    fast_u8 get_h() const { return get_high8(state.hl); }
    void set_h(fast_u8 h) { state.hl = make16(h, get_l()); }

    fast_u8 on_get_h() const { return get_h(); }
    void on_set_h(fast_u8 h) { set_h(h); }

    fast_u8 get_l() const { return get_low8(state.hl); }
    void set_l(fast_u8 l) { state.hl = make16(get_h(), l); }

    fast_u8 on_get_l() const { return get_l(); }
    void on_set_l(fast_u8 l) const { set_l(l); }

    fast_u8 get_a() const { return get_high8(state.af); }
    void set_a(fast_u8 a) { state.af = make16(a, get_f()); }

    fast_u8 on_get_a() const { return get_a(); }
    void on_set_a(fast_u8 a) { set_a(a); }

    fast_u8 get_f() const { return get_low8(state.af); }
    void set_f(fast_u8 f) { state.af = make16(get_a(), f); }

    fast_u8 on_get_f() const { return get_f(); }
    void on_set_f(fast_u8 f) { set_f(f); }

    fast_u8 get_ixh() const { return get_high8(state.ix); }
    void set_ixh(fast_u8 ixh) { state.ix = make16(ixh, get_ixl()); }

    fast_u8 on_get_ixh() const { return get_ixh(); }
    void on_set_ixh(fast_u8 ixh) { set_ixh(ixh); }

    fast_u8 get_ixl() const { return get_low8(state.ix); }
    void set_ixl(fast_u8 ixl) { state.ix = make16(get_ixh(), ixl); }

    fast_u8 on_get_ixl() const { return get_ixl(); }
    void on_set_ixl(fast_u8 ixl) const { set_ixl(ixl); }

    fast_u8 get_iyh() const { return get_high8(state.iy); }
    void set_iyh(fast_u8 iyh) { state.iy = make16(iyh, get_iyl()); }

    fast_u8 on_get_iyh() const { return get_iyh(); }
    void on_set_iyh(fast_u8 iyh) { set_iyh(iyh); }

    fast_u8 get_iyl() const { return get_low8(state.iy); }
    void set_iyl(fast_u8 iyl) { state.iy = make16(get_iyh(), iyl); }

    fast_u8 on_get_iyl() const { return get_iyl(); }
    void on_set_iyl(fast_u8 iyl) const { set_iyl(iyl); }

    fast_u16 get_hl() const { return state.hl; }
    void set_hl(fast_u16 hl) { state.hl = hl; }

    fast_u16 on_get_hl() const {
        // Always get the low byte first.
        fast_u8 l = (*this)->on_get_l();
        fast_u8 h = (*this)->on_get_h();
        return make16(h, l); }
    void on_set_hl(fast_u16 hl) {
        // Always set the low byte first.
        (*this)->on_set_l(get_low8(hl));
        (*this)->on_set_h(get_high8(hl)); }

    fast_u16 get_af() const { return state.af; }
    void set_af(fast_u16 af) { state.af = af; }

    fast_u16 on_get_af() const {
        // Always get the low byte first.
        fast_u8 f = (*this)->on_get_f();
        fast_u8 a = (*this)->on_get_a();
        return make16(a, f); }
    void on_set_af(fast_u16 af) {
        // Always set the low byte first.
        (*this)->on_set_f(get_low8(af));
        (*this)->on_set_a(get_high8(af)); }

    fast_u16 get_ix() const { return state.ix; }
    void set_ix(fast_u16 ix) { state.ix = ix; }

    fast_u16 on_get_ix() const {
        // Always get the low byte first.
        fast_u8 l = (*this)->on_get_ixl();
        fast_u8 h = (*this)->on_get_ixh();
        return make16(h, l); }
    void on_set_ix(fast_u16 ix) {
        // Always set the low byte first.
        (*this)->on_set_ixl(get_low8(ix));
        (*this)->on_set_ixh(get_high8(ix)); }

    fast_u16 get_iy() const { return state.iy; }
    void set_iy(fast_u16 iy) { state.iy = iy; }

    fast_u16 on_get_iy() const {
        // Always get the low byte first.
        fast_u8 l = (*this)->on_get_iyl();
        fast_u8 h = (*this)->on_get_iyh();
        return make16(h, l); }
    void on_set_iy(fast_u16 iy) {
        // Always set the low byte first.
        (*this)->on_set_iyl(get_low8(iy));
        (*this)->on_set_iyh(get_high8(iy)); }

    fast_u16 get_disp_target(fast_u16 base, fast_u8 d) {
        return !get_sign8(d) ? add16(base, d) : sub16(base, neg8(d));
    }

    fast_u8 read_at_disp(fast_u8 d, bool long_read_cycle = false) {
        index_reg ip = (*this)->get_index_reg();
        fast_u16 addr = get_disp_target(get_index_reg_value(ip), d);
        fast_u8 res = long_read_cycle ? (*this)->on_read4_cycle(addr) :
                                        (*this)->on_read3_cycle(addr);
        if(ip != index_reg::hl)
            (*this)->on_set_memptr(addr);
        return res;
    }

    fast_u8 on_get_r(reg r, fast_u8 d, bool long_read_cycle = false) {
        switch(r) {
        case reg::b: return (*this)->on_get_b();
        case reg::c: return (*this)->on_get_c();
        case reg::d: return (*this)->on_get_d();
        case reg::e: return (*this)->on_get_e();
        case reg::h: return (*this)->on_get_h();
        case reg::l: return (*this)->on_get_l();
        case reg::at_hl: return read_at_disp(d, long_read_cycle);
        case reg::a: return (*this)->on_get_a();
        }
        assert(0);
    }

    fast_u16 get_index_reg_value(index_reg ip) const {
        switch(ip) {
        case index_reg::hl: return (*this)->on_get_hl();
        case index_reg::ix: return (*this)->on_get_ix();
        case index_reg::iy: return (*this)->on_get_iy();
        }
        assert(0);
    }

    fast_u16 get_pc() const { return state.pc; }
    void set_pc(fast_u16 pc) { state.pc = pc; }

    fast_u16 on_get_pc() const { return get_pc(); }
    void on_set_pc(fast_u16 pc) { set_pc(pc); }

    fast_u16 get_pc_on_fetch() const { return (*this)->on_get_pc(); }
    void set_pc_on_fetch(fast_u16 pc) { (*this)->on_set_pc(pc); }

    fast_u16 get_memptr() const { return state.memptr; }
    void set_memptr(fast_u16 memptr) { state.memptr = memptr; }

    fast_u16 on_get_memptr() const { return get_memptr(); }
    void on_set_memptr(fast_u16 memptr) { set_memptr(memptr); }

    bool get_iff1() const { return state.iff1; }
    void set_iff1(bool iff1) { state.iff1 = iff1; }

    bool on_get_iff1() const { return get_iff1(); }
    void on_set_iff1(bool iff1) { set_iff1(iff1); }

    bool get_iff1_on_di() const { return (*this)->on_get_iff1(); }
    void set_iff1_on_di(bool iff1) { (*this)->on_set_iff1(iff1); }

    bool get_iff2() const { return state.iff2; }
    void set_iff2(bool iff2) { state.iff2 = iff2; }

    bool on_get_iff2() const { return get_iff2(); }
    void on_set_iff2(bool iff2) { set_iff2(iff2); }

    bool get_iff2_on_di() const { return (*this)->on_get_iff2(); }
    void set_iff2_on_di(bool iff2) { (*this)->on_set_iff2(iff2); }

    fast_u16 get_last_fetch_addr() const { return state.last_fetch_addr; }

    fast_u8 on_fetch_at(fast_u16 addr) {
        (*this)->tick(4);
        return (*this)->on_access(addr);
    }

    void do_alu(alu k, fast_u8 n) {
        fast_u16 af = (*this)->on_get_af();
        fast_u8 a = get_high8(af);
        fast_u8 f = get_low8(af);
        switch(k) {
        case alu::add: assert(0); break;  // TODO
        case alu::adc: assert(0); break;  // TODO
        case alu::sub: assert(0); break;  // TODO
        case alu::sbc: assert(0); break;  // TODO
        case alu::and_a: assert(0); break;  // TODO
        case alu::xor_a:
            a ^= n;
            f = (a & (sf_mask | yf_mask | xf_mask)) | zf_ari(a) | pf_log(a);
            break;
        case alu::or_a: assert(0); break;  // TODO
        case alu::cp: assert(0); break;  // TODO
        }
        (*this)->on_set_af(make16(a, f));
    }

    void on_alu_r(alu k, reg r, fast_u8 d) {
        do_alu(k, (*this)->on_get_r(r, d)); }
    void on_di() { (*this)->set_iff1_on_di(false);
                   (*this)->set_iff2_on_di(false); }
    void on_nop() {}

    fast_u8 on_fetch_cycle() {
        fast_u16 pc = (*this)->get_pc_on_fetch();
        fast_u8 op = (*this)->on_fetch_at(pc);
        state.last_fetch_addr = pc;
        (*this)->set_pc_on_fetch(inc16(pc));
        return op;
    }

    fast_u8 on_read3_cycle(fast_u16 addr) {
        (*this)->tick(3);
        return (*this)->on_access(addr);
    }

    fast_u8 on_read4_cycle(fast_u16 addr) {
        (*this)->tick(4);
        return (*this)->on_access(addr);
    }

    void on_exec5_cycle(fast_u16 addr) {
        unused(addr);
        (*this)->tick(5);
    }

    void on_step() { (*this)->decode(); }
    void step() { return (*this)->on_step(); }

protected:
    D *operator -> () { return static_cast<D*>(this); }
    const D *operator -> () const { return static_cast<const D*>(this); }
};

}  // namespace z80

#endif  // Z80_H
