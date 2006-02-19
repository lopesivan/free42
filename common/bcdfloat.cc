/**
 * Copyright (c) 2005 voidware ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "bcdfloat.h"

/* tempoary storage for rounding for printing */
BCDFloat BCDFloat::roundedVal_;
BCDFloat BCDFloat::rounding_;
int BCDFloat::decade_[4] = { 1000, 100, 10, 1 };
short BCDFloat::posInfD_[P+1] = { 0, 0, 0, 0, 0, 0, 0, 0x3fff };
short BCDFloat::negInfD_[P+1] = { 0, 0, 0, 0, 0, 0, 0, 0xBfff };
short BCDFloat::nanD_[P+1] =    { 0, 0, 0, 0, 0, 0, 0, 0x3000 };

void BCDFloat::_init()
{
    int i;
    for (i = 0; i <= P; ++i) {
        d_[i] = 0;
    }
}

BCDFloat::BCDFloat(const char* s)
{
    /* make a BCD float from a string.
     */

    _init();

    bool neg = false;
    if (*s == '-') {
        neg = true;
        ++s;
    }

    int i;
    int j = 0;
    int e = 0;
    
    /* find the end of the input string */
    const char* endp = s;
    const char* startp = 0;
    const char* point = 0;
    bool begun = false;
    while (*endp && toupper(*endp) != 'E') {
        if (*endp == '.') {
            if (point) break;
            point = endp;
            if (!startp) {
                startp = endp;
                begun = true;
            }
        }
        else {
            if (!isdigit(*endp)) break;
            if (*endp != '0' && !begun) {
                startp = endp;
                begun = true;
            }
        }
        ++endp;
    }

    bool eneg = false;
    if (startp) {
        if (toupper(*endp) == 'E') {
            /* accept 4 digits of exponent */
            const char* p = endp + 1;
            if (*p == '-') {
                eneg = true;
                ++p;
            }
            else if (*p == '+') ++p;  // allow E+1234 

            e = 0;
            const char* se = p;
            for (i = 0; i < 4; ++i) {
                if (isdigit(*p)) ++p;
                else break;
            }
            if (p > se) {
                i = 4 - (p-se); // decade of start digit.
                while(se != p) e += ((*se++)-'0')*decade_[i++];
            }
            if (eneg) e = -e;
        }

        /* represent the decimal point by adjusting the exponent */
        if (point) {
            e += point - startp;
        }
        else {
            e += endp - startp;
        }

        /* calculate the decade offset of the exponent remainder */
        if (e >= 0) {
            i = (e&3);
            if (i) i = 4-i;
        }
        else {
            i = ((-e) & 3);
        }
        e += i;

        /* convert to 4dec */
        e >>= 2;
    
        const char* p = startp;
        while (p != endp) {
            int d = 0;
            while (i < 4) {
                if (*p != '.') {
                    d += (*p - '0')*decade_[i];
                    ++i;
                }
                ++p;
                if (p == endp) break;
            }
            i = 0;
            d_[j] = d;
            if (j == P) {
                if (_round()) ++e;
                break;  // full up.
            }
            ++j;
        }
        if (e <= -EXPLIMIT) _init();
        else {
            if (e > EXPLIMIT) *this = posInf();
            else exp(e);
            if (neg) negate();
        }
    }
}

BCDFloat::BCDFloat(int v)
{
    _init();

    // Special case to prevent zero with nonzero exponent
    if (v == 0)
	return;
    
    bool neg = false;
    if (v < 0) {
        neg = true;
        v = -v;
    }

    /* quicker to deal with cases separately */
    if (v < BASE) {
        d_[0] = v;
        d_[P] = 1;
    }
    else if (v < BASE*BASE) {
        d_[0] = v/BASE;
        d_[1] = v - d_[0]*BASE;
        d_[P] = 2;
    }
    else {
        d_[0] = v/(BASE*BASE);
        v -= d_[0]*(BASE*BASE);
        d_[1] = v/BASE;
        d_[2] = v - d_[1]*BASE;
        d_[P] = 3;
    }

    if (neg) negate();
}

const BCDFloat& BCDFloat::_round20() const
{
    if (isSpecial()) return *this;

    /* round to 20 decimal digits */
    /* first see how many digits in the first `digit' */

    int dn;
    if (d_[0] >= 1000) dn = 5000;
    else if (d_[0] >= 100) dn = 500;
    else if (d_[0] >= 10) dn = 50;
    else dn = 5;
    
    rounding_.d_[0] = dn;
    int e = exp();
    rounding_.exp(e-5);
    if (neg()) rounding_.negate();
    add(this, &rounding_, &roundedVal_);
    int v = roundedVal_.d_[5];

    roundedVal_.d_[P-1] = 0;
    if (dn == 5000) v = 0;
    else if (dn == 500)  v = (v/1000)*1000;
    else if (dn == 50) v = (v/100)*100;
    else v = (v/10)*10;
    roundedVal_.d_[5] = v;

    return roundedVal_;
}

void BCDFloat::asString(char* buf) const
{
    const BCDFloat& val = _round20();
    val._asString(buf);
}

void BCDFloat::_asString(char* buf) const
{
    char* p = buf;
    if (isSpecial()) {
        if (isNan()) strcpy(p, "NaN");
        else { // inf
            if (neg()) *p++ = '-';
            strcpy(p, "Inf");
        }
    }
    else {
        int i;
        if (neg()) *p++ = '-';
        char* point = 0;

        int v;
        int eadj = 0;
        bool scimode = false;
        int e = exp();
        i = 0;
        if (e > 0) {
            /* if the exponent is less than our digits, we can
             * print out the number like an integer.
             */
            if (e <= 5) {
                while (i < e) {
                    v = d_[i];            
                    if (!i) sprintf(p, "%d", v);            
                    else sprintf(p, "%04d", v);            
                    while (*p) ++p;
                    ++i;
                }
                e -= i;
            }
            else {
                /* otherwise we have a larger number. print out
                 * as scientific form.
                 */
                v = d_[0];
                char tb[8];
                sprintf(tb, "%d", v);
                char* q = tb;
                *p++ = *q++;
                *p = '.'; point = p; ++p;
                while (*q) {
                    *p++ = *q++;
                    ++eadj;
                }
                ++i;
                e -= 1;
                scimode = true;
            }
        }
        else {
            /* otherwise have small number */
            *p++ = '0';
            *p = 0;
        }

        int n = P;
        while (!d_[n-1] && n > i) --n;

        if (i < n) {
            if (!scimode) {
                *p = '.';
                point = p;
                ++p;
            }
            for (; i < n; ++i) {
                int v = d_[i];
                sprintf(p, "%04d", v);            
                p += 4;
            }
        }

        /* tidy up */
        if (point) {
            while (p > point && (p[-1] == '0' || p[-1] == '.')) *--p = 0;
        }

        if (e) {
            sprintf(p, "e%d", e*4+eadj);
            while (*p) ++p;
        }
    }
}

int BCDFloat::_round()
{
    int v;
    int i;
    int r = (d_[P] >= BASE/2);
    if (r) {
        for (i = P-1; i >= 0; --i) {
            v = d_[i] + r;
            r = 0;
            if (v >= BASE) {
                r = 1;
                v -= BASE;
            }
            d_[i] = v;
        }

        if (r) {
            /* rounding generated overall carry */
            _rshift();
            d_[0] = r;
        }
    }
    return r;
}

void BCDFloat::add(const BCDFloat* a, const BCDFloat* b, BCDFloat* c)
{
    if (a->isSpecial() || b->isSpecial()) {
        /* inf + inf = inf
         * -inf + (-inf) = -inf
         */
        bool done = false;
        if (!a->isNan() && !b->isNan()) {
            if (a->isInf()) {
                if (b->isInf()) {
                    if (a->d_[P] == b->d_[P]) { *c = *a; done = true; }
                }
                else {
                    *c = *a;
                    done = true; // inf + x = inf
                }
            }
            else { // b is inf
                *c = *b;
                done = true; // x + inf = inf
            }
        }

        if (!done) {
            /* all others -> nan */
            *c = nan();
        }
        return;
    }

    int ea = a->exp();
    int eb = b->exp();

    bool na = a->neg();
    bool nb = b->neg();
    bool sub = na != nb;

    if (sub) {
        if (ea >= eb) {
            _usub(a, b, c);
            if (na) c->negate();
        }
        else {
            _usub(b, a, c);
            if (nb) c->negate();
        }
    }
    else {
        if (ea >= eb) _uadd(a, b, c);
        else _uadd(b, a, c);

        if (na) c->negate();
    }
}

void BCDFloat::sub(const BCDFloat* a, const BCDFloat* b, BCDFloat* c)
{
    if (a->isSpecial() || b->isSpecial()) {
        bool done = false;
        if (!a->isNan() && !b->isNan()) {
            if (a->isInf()) {
                if (b->isInf()) {
                    /* -inf - (inf) = -inf
                     * inf - (-inf) = inf
                     */
                    if (a->d_[P] != b->d_[P]) { *c = *a; done = true; }
                }
                else { // a inf && !b inf
                    *c = *a;
                    done = true; // inf - x = inf
                }
            }
            else { // b is inf
                *c = *b;
                c->negate();
                done = true; // x - inf = -inf
            }
        }

        if (!done) {
            /* all others -> nan */
            *c = nan();
        }
        return;
    }

    bool na = a->neg();
    bool nb = b->neg();
    bool sub = (na == nb);

    int ea = a->exp();
    int eb = b->exp();

    if (sub) {
        if (ea >= eb) {
            _usub(a, b, c);
            if (na) c->negate();
        }
        else {
            _usub(b, a, c);
            if (!na) c->negate();
        }
    }
    else {
        if (ea >= eb) _uadd(a, b, c);
        else _uadd(b, a, c);

        if (na) c->negate();
    }
}

void BCDFloat::_uadd(const BCDFloat* a, const BCDFloat* b, BCDFloat* c)
{
    int ea = a->exp();
    int eb = b->exp();

    int d = ea - eb;
    if (d >= P) {
        /* `b' is insignificant */
        *c = *a;
    }
    else {
        int i;
        int ca = 0;
        int v;
        for (i = P-1; i >= 0; --i) {
            v = a->d_[i] + ca;
            int j = i-d;
            if (j >= 0) v += b->d_[j];
            ca = 0;
            if (v >= BASE) {
                ca = 1;
                v -= BASE;
            }
            c->d_[i] = v;
        }

        if (ca) {
            /* overall carry, shift down and round */
            c->_rshift();
            c->d_[0] = ca;
            if (c->_round()) ++ea;
            ++ea;
        }

        if (ea > EXPLIMIT) *c = posInf();
        else c->exp(ea);
    }
}

void BCDFloat::_usub(const BCDFloat* a, const BCDFloat* b, BCDFloat* c)
{
    int ea = a->exp();
    int eb = b->exp();
    bool neg = false;

    int d = ea - eb;
    if (d >= P) {
        /* `b' is insignificant */
        *c = *a;
    }
    else {
        int i;
        int ca = 0;
        int v;
        for (i = P-1; i >= 0; --i) {
            v = a->d_[i];
            int j = i-d;
            if (j >= 0) v -= b->d_[j];
            v -= ca;
            ca = 0;
            if (v < 0) {
                ca = 1;
                v += BASE;
            }
            c->d_[i] = v;
        }

        int e = a->exp();
        if (ca) {
            /* overall borrow, need to complement number */
            ca = 1;
            for (i = P-1; i >= 0; --i) {
                v = BASE-1 - c->d_[i] + ca;
                ca = 0;
                if (v >= BASE) {
                    ca = 1;
                    v -= BASE;
                }
                c->d_[i] = v;
            }
            neg = true;
        }

        i = 0;
        for (;;) {
            if (c->d_[i]) {
                if (i) {
                    e -= i;
                    int j = 0;
                    while (i < P) c->d_[j++] = c->d_[i++];
                }
                break;
            }
            if (++i == P) {
                /* is zero */
                e = 0;
                break;
            }
        }

        if (e > EXPLIMIT) *c = posInf();
        else c->exp(e);
        if (neg) c->negate();
    }
}

void BCDFloat::mul(const BCDFloat* a, const BCDFloat* b, BCDFloat* c)
{
    int na = a->neg();
    int nb = b->neg();

    if (a->isSpecial() || b->isSpecial()) {
        bool done = false;
        if (!a->isNan() && !b->isNan()) {
            if (a->isInf()) {
                if (b->isInf()) {
                    *c = posInf();
                    done = true;
                }
                else {
                    if (!b->isZero()) { // inf * 0 = nan
                        *c = posInf();
                        done = true; // inf * x = inf, x != 0
                    }
                }
            }
            else { // b is inf
                if (!a->isZero()) { // 0 * inf = nan
                    *c = posInf();
                    done = true; // x * inf = inf, x != 0
                }
            }
        }

        if (!done) {
            /* all others -> nan */
            *c = nan();
        }
        else {
            if (na != nb) c->negate();            
        }
        return;
    }

    int ca;
    int i, j;
    int u, v;

    int ea = a->exp();
    int eb = b->exp();


    BCDFloat acc;
    c->_init();

    int cc = 0;
    for (i = P-1; i >= 0; --i) {
        ca = c->_round();
        if (!ca) c->_rshift();
        c->d_[0] = cc + ca;
        cc = 0;
        
        u = a->d_[i];
        if (!u) continue;

        ca = 0;
        for (j = P; j > 0; --j) {
            v = b->d_[j-1] * u + ca;
            ca = 0;
            if (v >= BASE) {
                ca = v / BASE;
                v = v - ca*BASE;
            }
            acc.d_[j] = v;
        }
        acc.d_[0] = ca;

        /* now add acc into c */
        for (j = P; j >= 0; --j) {
            v = c->d_[j] + acc.d_[j] + cc;
            cc = 0;
            if (v >= BASE) {
                cc = 1;
                v -= BASE;
            }
            c->d_[j] = v;
        }

        /* any total overflow into cc, will go into `c' next 
         * time we shift down.
         */
    }

    c->d_[0] += cc;  // carry?

    if (!c->d_[0]) {
        c->_lshift();
    }
    else {
        c->_round();  // shouldnt carry.
        ++ea;
    }

    ea += eb - 1;
    if (ea <= -EXPLIMIT) c->_init();
    else {
        if (ea > EXPLIMIT) *c = posInf();
        else c->exp(ea);

        /* fix sign */
        if (na != nb) c->negate();
    }
}

void BCDFloat::div(const BCDFloat* a, const BCDFloat* b, BCDFloat* c)
{
    int na = a->neg();
    int nb = b->neg();

    if (a->isSpecial() || b->isSpecial()) {
        bool done = false;
        if (!a->isNan() && !b->isNan()) {
            if (a->isInf()) {
                if (b->isInf()) {
                    /* inf / inf = nan */
                }
                else {
                    *c = posInf();
                    if (na != nb) c->negate();            
                    done = true; // inf / x = inf
                }
            }
            else { // b is inf
                /* x/inf = 0 */
                c->_init();
                done = true;
            }
        }

        if (!done) {
            /* all others -> nan */
            *c = nan();
        }
        return;
    }

    int u, v;
    int ca;
    int j = 0;
    int i;
    int q;

    bool az = a->isZero();
    bool bz = b->isZero();
    if (az || bz) {
        if (az) {
            if (bz) *c = nan();
            else c->_init();
        }
        else { // bz && !az
            *c = posInf();
            if (a->neg()) c->negate();
        }
    }
    else {
        int ea = a->exp();
        int eb = b->exp();


        BCDFloat acc;
        BCDFloat b1;

        u = BASE/(b->d_[0]+1);

        if (u != 1) {
            /* prenormialise `a' and move into acc using spare digit */
            ca = 0;
            for (i = P; i > 0; --i) {
                v = a->d_[i-1]*u + ca;
                ca = 0;
                if (v >= BASE) {
                    ca = v/BASE;
                    v -= ca*BASE;
                }
                acc.d_[i] = v;
            }
            acc.d_[0] = ca;

            /* prenormalise `b' into b1 */
            ca = 0;
            for (i = P-1; i >= 0; --i) {
                v = b->d_[i]*u + ca;
                ca = 0;
                if (v >= BASE) {
                    ca = v/BASE;
                    v -= ca*BASE;
                }
                b1.d_[i] = v;
            }
        }
        else {
            /* u is often 1 */
            for (i = P-1; i >= 0; --i) {
                acc.d_[i+1] = a->d_[i];
                b1.d_[i] = b->d_[i];
            }
            acc.d_[0] = 0;
        }

        for (;;) {
            if (acc.d_[0] == b1.d_[0]) q = BASE-1;
            else {
                v = acc.d_[0]*BASE + acc.d_[1];
                q = v/b1.d_[0];

                while (b1.d_[1]*q > ((v - q*b1.d_[0])*BASE + acc.d_[2])) {
                    --q;
                }
            }

            if (!q && !j) {
                /* first quotient digit is zero. can gain extra
                 * accuracy by ignoring this and adjusting exponent.
                 */
                --ea;
            }
            else {
                c->d_[j] = q;
                if (j == P) break;
                ++j;

                ca = 0;
                for (i = P; i > 0; --i) {
                    v = acc.d_[i] - b1.d_[i-1]*q - ca;
                    ca = 0;
                    if (v < 0) {
                        ca = (-v + BASE-1)/BASE;
                        v += ca*BASE;
                    }
                    acc.d_[i] = v;
                }
                v = acc.d_[0] - ca;

                if (v) {
                    /* the infamous add back correction */
                    ca = 0;
                    for (i = P; i > 0; --i) {
                        v = BASE - acc.d_[i] + b1.d_[i-1] + ca;
                        ca = 0;
                        if (v >= BASE) {
                            ca = 1;
                            v -= BASE;
                        }
                        acc.d_[i] = v;
                    }
                }

            }

            acc._lshift();
            acc.d_[P] = 0;
        }

        c->_round();
        ea -= eb - 1;
        if (ea <= -EXPLIMIT) c->_init();
        else {
            if (ea > EXPLIMIT) *c = posInf();
            else c->exp(ea);
            if (na != nb) c->negate();
        }
    }
}

void BCDFloat::mul2(unsigned short* ad, int ea,
                    unsigned short* bd, int eb,
                    unsigned short* cd, int& ec)
{
    int ca;
    int i, j;
    int u, v;

    unsigned short acc[2*P+1];

    for (i = 0; i < 2*P; ++i) cd[i] = 0;
    int cc = 0;
    for (i = 2*P-1; i >= 0; --i) {
        for (j = 2*P; j > 0; --j) cd[j] = cd[j-1];
        cd[0] = cc; cc = 0;
        u = ad[i];
        if (!u) continue;

        ca = 0;
        for (j = 2*P; j > 0; --j) {
            v = bd[j-1]*u + ca;
            ca = 0;
            if (v >= BASE) {
                ca = v / BASE;
                v = v - ca*BASE;
            }
            acc[j] = v;
        }
        acc[0] = ca;

        /* now add acc into c */
        for (j = 2*P; j >= 0; --j) {
            v = cd[j] + acc[j] + cc;
            cc = 0;
            if (v >= BASE) {
                cc = 1;
                v -= BASE;
            }
            cd[j] = v;
        }

        /* any total overflow into cc, will go into `c' next 
         * time we shift down.
         */
    }
    cd[0] += cc;  // carry?
    ec = ea + eb;
}

static int root0(int v)
{
    /* integer root for x<= 9999 */
    int x = 0;
    int b = 1<<7;
    do {
        x ^= b;  
        if (x*x > v ) x ^= b;
    } while (b >>=1);
    return x;
}

bool BCDFloat::sqrt(const BCDFloat* a, BCDFloat* r)
{
    if (a->neg()) return false;
    if (a->isInf()) {
        *r = *a;  // sqrt(inf) = inf
        return true;
    }
    if (a->isNan()) return false;

    BCDFloat acc;
    BCDFloat t;
    int rs;
    int as;
    int ts;
    int v;
    int rodd;
    int q;

    BCDFloat u;
    int us = 0;

    r->_init();

    int e = a->exp();

    v = root0(a->d_[0]);

    rodd = !(e & 1);
    r->d_[0] = v;
    if (rodd) r->d_[0] *= 100;
    rs = 1;

    as = 0;
    acc.d_[0] = a->d_[0] - v*v;
    if (acc.d_[0]) ++as;

    int i;
    int j = 1;
    int ca;

    for (;;) {
        /* bring in the next digit */
        acc.d_[as] = j < P ? a->d_[j++] : 0;

        q = 0;
        if (acc.d_[0]) {
            ++as;
        
            /* t = 200*r if even, t=2*r if odd */
            int m = rodd ? 2 : 200;
            ca = 0;
            ts = rs;
            for (i = rs; i > 0; --i) {
                v = r->d_[i-1]*m + ca;
                ca = 0;
                if (v >= BASE) {
                    ca = v/BASE;
                    v -= ca*BASE;
                }
                t.d_[i] = v;
            }
            t.d_[i] = ca;
            ++ts;

            while (!t.d_[0]) {
                for (i = 0; i < ts; ++i) t.d_[i] = t.d_[i+1];
                --ts;
            }

            if (ts > P) {
                /* rarely, the tempory size can become bigger than
                 * we can handle. this can only happen on the last
                 * digit. if so, stop.
                 */
                break;
            }

            q = 0;
            if (ts == as) {
                q = (acc.d_[0]*BASE + acc.d_[1])/(t.d_[0]*BASE+t.d_[1]);
            }
            else if (as > ts) {
                q = (acc.d_[0]*BASE + acc.d_[1])/t.d_[0];
            }

            if (q) {
                if (q > 99) q = 99;
        
                /* t = t + q */
                t.d_[ts-1] += q;  // cant carry

                for (;;) {
                    /* u = t*q */
                    ca = 0;
                    us = ts;
                    for (i = ts; i > 0; --i) {
                        v = t.d_[i-1]*q + ca;
                        ca = 0;
                        if (v >= BASE) {
                            ca = v/BASE;
                            v -= ca*BASE;
                        }
                        u.d_[i] = v;
                    }
                    u.d_[i] = ca;
                    if (ca) ++us;
                    else {
                        for (i = 0; i < us; ++i) u.d_[i] = u.d_[i+1];
                    }
                
                    /* determine whether u > acc. if so then q was too
                     * big.
                     */
                    bool fail = us > as;
                    if (!fail && us == as) {
                        for (i = 0; i < as; ++i) {
                            int d = u.d_[i] - acc.d_[i];
                            if (d > 0) {
                                fail = true;
                                break;
                            }
                            else if (d < 0) {
                                break;
                            }
                        }
                    }

                    if (!fail) break;

                    /* decrease q by 1 and try again */
                    q -= 1;
                    --t.d_[ts-1]; // adjust for new q
                }
            }
        }


        if (rodd) {
            /* can accommodate 2 more digits in current size */
            r->d_[rs-1] += q;
            rodd = 0;
            if (rs == P) break;
        }
        else {
            r->d_[rs++] = q*100;
            rodd = 1;
        }
        
        if (q) {
            /* acc = acc - u.
             * wont borrow because u <= acc.
             */
            int k;
            ca = 0;
            k = us;
            for (i = as-1; i >= 0; --i) {
                v = acc.d_[i] - ca;
                if (k > 0) v -= u.d_[--k];
                ca = 0;
                if (v < 0) {
                    v += BASE;
                    ca = 1;
                }
                acc.d_[i] = v;
            }

            while (!acc.d_[0]) {
                for (i = 0; i < as; ++i) acc.d_[i] = acc.d_[i+1];            
                if (!--as) break;
            }
        }
    }
    r->exp((e+1)/2);
    return true;
}

bool BCDFloat::lt(const BCDFloat* a, const BCDFloat* b)
{
    /* true iff a < b */
    BCDFloat c;
    sub(a, b, &c);
    return c.neg();
}

bool BCDFloat::le(const BCDFloat* a, const BCDFloat* b)
{
    /* true iff a <= b */
    BCDFloat c;
    sub(a, b, &c);
    return c.isZero() || c.neg();
}

bool BCDFloat::equal(const BCDFloat* a, const BCDFloat* b)
{
    /* handle zero separately, to prevent zeroes with unequal
     * exponents from being considered different.
     * TODO - bug report. (ThO)
     */
    if (a->d_[0] == 0 && b->d_[0] == 0)
	return true;

    /* compare the memory */
    int i;
    for (i = 0; i <= P; ++i) {
        if (a->d_[i] != b->d_[i]) return false;
    }
    return true;
}

bool BCDFloat::trunc(const BCDFloat* a, BCDFloat* c)
{
    /* truncate towards zero.
     * trunc(2.1) = 2.
     * trunc(-2.1) = -2
     */
    *c = *a;
    int e = c->exp();
    int i;
    for (i = P-1; i >= 0; --i) {
        if (i >= e) {
            c->d_[i] = 0;
        }
    }
    return true;
}

bool BCDFloat::floor(const BCDFloat* a, BCDFloat* c)
{
    /* floor, largest integer <= a.
     * eg floor(2.1) = 2.
     *    floor(-2.1) = -3.
     */

    *c = *a;
    int e = c->exp();
    int i;
    bool changed = false;
    for (i = P-1; i >= 0; --i) {
        if (i >= e) {
            if (c->d_[i]) changed = true;
            c->d_[i] = 0;
        }
    }
    
    if (c->neg() && changed) {
        /* need to subtract 1 */
        for (i = P-1; i >= 0; --i) {
            if (c->d_[i]) {
                ++c->d_[i];
                break;
            }
        }
    }
    return true;
}

int BCDFloat::ifloor(const BCDFloat* x)
{
    BCDFloat a;
    floor(x, &a);

    int na = a.neg();
    int ea = a.exp();

    int v = 0;
    int i = 0;
    while (i < ea && i < P) {
        if (v > 214748) return 0; // too large, bail out.
        v*= BASE;
        v += a.d_[i];
        ++i;
    }
    if (na) v = -v;
    return v;
}

bool BCDFloat::isInteger() const
{
    int e = exp();
    int i;
    for (i = P-1; i >= 0; --i) {
        if (d_[i]) {
            return e > i;
        }
    }
    return false;
}

