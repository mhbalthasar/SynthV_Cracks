/*
 * Dreamtonics activation code utilities for JavaScript 1.0
 *
 * Copyright (C) 2023 Xi Jinpwned Software
 *
 * This software is made available to you under the terms of the GNU Affero
 * General Public License version 3.0. For more information, see the included
 * LICENSE.txt file.
 */

'use strict';

class DTAuthUtil {
  static b36toi(encodedData) {
    let value = 0n, step = 1n;
    encodedData = encodedData.toUpperCase().replaceAll(/[^A-Z0-9]/g, '');

    for (let i = 0; i < encodedData.length; i++) {
      let c = encodedData.charCodeAt(i);
      let v = BigInt(c >= 65 ? c - 65 + 10 : c - 48);
      value += step * v;
      step *= 36n;
    }

    return value;
  }

  static itob36(decodedInt) {
    let ret = [];

    while (decodedInt != 0n) {
      let v = Number(decodedInt % 36n);
      let c = String.fromCharCode(v < 10 ? v + 48 : v - 10 + 65);
      ret.push(c);
      decodedInt /= 36n;
    }

    return ret.join('');
  }

  /* async */ static sha256(data) {
    if (data.buffer) data = data.buffer;
    return crypto.subtle.digest("SHA-256", data);
  }

  static biGetMsb(bi) {
    let i = 0;
    while (bi > 0n) {
      bi >>= 1n;
      i++;
    }
    return i;
  }

  static biGetBits(bi, startBit, numBits) {
    return (bi >> BigInt(startBit)) & ((1n << BigInt(numBits)) - 1n);
  }

  static biSetBit(bi, bit, value) {
    if ((value && !DTAuthUtil.biGetBits(bi, bit, 1)) ||
        (!value && DTAuthUtil.biGetBits(bi, bit, 1)))
      bi ^= (1n << BigInt(bit));
    return bi;
  }

  static biToBytes(bi, pad = false) {
    let msb = DTAuthUtil.biGetMsb(bi),
        len = (msb + 7) >> 3,
        padlen = pad ? (((len + 3) / 4) | 0) * 4 : len,
        arr = new Uint8Array(padlen);

    for (let i = 0; i < len; i++) {
      arr[i] = Number(bi & 0xFFn);
      bi >>= 8n;
    }

    return arr;
  }
}

class DTAuthCode {
  static MIN_CODE_BI = DTAuthUtil.b36toi("00000-00000-00000-00000-00000");
  static MAX_CODE_BI = DTAuthUtil.b36toi("ZZZZZ-ZZZZZ-ZZZZZ-ZZZZZ-ZZZZZ");

  static async checkCode(codeBi, extendedCheck = false) {
    const sha256  = DTAuthUtil.sha256,
          toBytes = DTAuthUtil.biToBytes,
          getBits = DTAuthUtil.biGetBits,
          getBitsInt = (x, y, z) => Number(getBits(x, y, z));

    let msb = DTAuthUtil.biGetMsb(codeBi);
    let hiBit = msb > 100 ? 100 : msb < 0 ? 0 : msb;

    let hash = await sha256(toBytes(getBits(codeBi, 0, hiBit)));
    hash = new Uint8Array(hash);

    if (hash[0] != 0 || (hash[1] & 3) != 0)
      return false;
    else if (!extendedCheck)
      return true;

    const checkBit = startBit => ((codeBi >> BigInt(startBit)) & 1n) != 0n;

    let bitSum = 0;
    for (let i = 0; i < 96; i++) {
      bitSum += checkBit(i) ? 1 : 0;
    }

    if (checkBit(96) != !!(~bitSum & 1))
      return false;

    if (checkBit(97) != (bitSum - (~~(bitSum / 13) + ~~(bitSum / 13) * 12)) < 7)
      return false;

    if ((checkBit(98)  != bitSum % 7  < 4 ) ||
        (checkBit(99)  != bitSum % 23 < 12) ||
        (checkBit(128) == 0))
      return false;

    let a = getBitsInt(codeBi, 0,   8),
        b = getBitsInt(codeBi, 4,   8),
        c = getBitsInt(codeBi, 100, 4);
    return ((a * b + 9) & 0xF) == c;
  }

  static async generateCode(extendedCheck = false) {
    const fixExtended = () => {
      const checkBit = startBit => ((codeBi >> BigInt(startBit)) & 1n) != 0n;

      let a = Number(DTAuthUtil.biGetBits(codeBi, 0, 8)),
          b = Number(DTAuthUtil.biGetBits(codeBi, 4, 8));
      let c = (a * b + 9) & 0xF;

      codeBi |= 0xfn << 100n;
      codeBi ^= 0xfn << 100n;
      codeBi |= BigInt(c & 0xf) << 100n;

      let bitSum = 0;
      for (let i = 0; i < 96; i++) {
        bitSum += checkBit(i) ? 1 : 0;
      }

      codeBi = DTAuthUtil.biSetBit(codeBi, 96, !!(~bitSum & 1));
      codeBi = DTAuthUtil.biSetBit(codeBi, 97, (bitSum - (~~(bitSum / 13) + ~~(bitSum / 13) * 12)) < 7);

      codeBi = DTAuthUtil.biSetBit(codeBi, 98, bitSum % 7 < 4);
      codeBi = DTAuthUtil.biSetBit(codeBi, 99, bitSum % 23 < 12);
      codeBi = DTAuthUtil.biSetBit(codeBi, 128, 1);
    }

    const makeSeed = () => (
      [1, 2, 3, 4, 5].map(x => (Math.random() * 10).toString(36).replace('.', ''))
                     .join('').substring(0, 25)
    );

    let codeBi = DTAuthUtil.b36toi(makeSeed());

    while (!(await DTAuthCode.checkCode(codeBi, extendedCheck))) {
      codeBi++;

      if (extendedCheck) do {
        fixExtended();
        if (codeBi > DTAuthCode.MAX_CODE_BI) {
          codeBi = DTAuthUtil.b36toi(makeSeed());
          continue;
        }
        break;
      } while (1);
    }

    return codeBi;
  }

  codeBi = 0n;
  forSynthV = false;

  constructor(code, forSynthV = false) {
    if (typeof code === 'string')
      code = DTAuthUtil.b36toi(code);
    if (code)
      this.codeBi = code;
    this.forSynthV = forSynthV;
  }

  async generate() {
    this.codeBi = await DTAuthCode.generateCode(this.forSynthV);
  }

  async validate() {
    return await DTAuthCode.checkCode(this.codeBi, this.forSynthV);
  }

  toString() {
    return DTAuthUtil.itob36(this.codeBi);
  }
}

export { DTAuthUtil, DTAuthCode };
