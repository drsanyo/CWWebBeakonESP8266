#include "si5351.h"
#include "si_bcn2.h"

const uint8_t cwSymbTab2[][5] = {
  {1, 3},             // 0  A
  {3, 1, 1, 1},       // 1  B
  {3, 1, 3, 1},       // 2  C
  {3, 1, 1},          // 3  D
  {1},                // 4  E
  {1, 1, 3, 1},       // 5  F
  {3, 3, 1},          // 6  G
  {1, 1, 1, 1},       // 7  H
  {1, 1},             // 8  I
  {1, 3, 3, 3},       // 9  J
  {3, 1, 3},          // 10 K
  {1, 3, 1, 1},       // 11 L
  {3, 3},             // 12 M
  {3, 1},             // 13 N
  {3, 3, 3},          // 14 O
  {1, 3, 3, 1},       // 15 P
  {3, 3, 1, 3},       // 16 Q
  {1, 3, 1},          // 17 R
  {1, 1, 1},          // 18 S
  {3},                // 19 T
  {1, 1, 3},          // 20 U
  {1, 1, 1, 3},       // 21 V
  {1, 3, 3},          // 22 W
  {3, 1, 1, 3},       // 23 X
  {3, 1, 3, 3},       // 24 Y
  {3, 3, 1, 1},       // 25 Z
  {3, 3, 3, 3, 3},    // 26 0
  {1, 3, 3, 3, 3},    // 27 1
  {1, 1, 3, 3, 3},    // 28 2
  {1, 1, 1, 3, 3},    // 29 3
  {1, 1, 1, 1, 3},    // 30 4
  {1, 1, 1, 1, 1},    // 31 5
  {3, 1, 1, 1, 1},    // 32 6
  {3, 3, 1, 1, 1},    // 33 7
  {3, 3, 3, 1, 1},    // 34 8
  {3, 3, 3, 3, 1}     // 35 9
};


si_bcn2::si_bcn2()
{
   l_msg = NULL;
   cbRfOn = NULL;
   cbRfOff = NULL;
   
   cwSymCnt = 0;
   cwSymTxCompleted = false;
   cwSymAllCompleted = false;

   msgCnt = 0;
   msgAllCompleted = false;
}

void si_bcn2::setParams(void (*rfOn)(), void (*rfOff)())
{
   cbRfOn  = rfOn;
   cbRfOff = rfOff;

   msgCnt = 0;
   msgAllCompleted = false;
   cwSymAllCompleted = false;
}

void si_bcn2::setText(char* msg)
{
   l_msg = msg;

   msgCnt = 0;
   msgAllCompleted = false;
   cwSymAllCompleted = false;
}


uint8_t si_bcn2::getIndex(char ch)
{
    if ((ch >= 65) && (ch <= 90))  return ch - 65;  // A - Z
    if ((ch >= 97) && (ch <= 122)) return ch - 97;  // a - z
    if ((ch >= 48) && (ch <= 57))  return ch - 22;  // 0 - 9
    return 255;
}

void si_bcn2::processOneSym()
{
  if (cwPassCnt > 0)
  {
    cwPassCnt--;
    return;
  }
  if (cwSymAllCompleted) return;

  if (!cwSymTxCompleted)
  {
    (*cbRfOn)();
    cwPassCnt = cwSymbTab2[cwIndex][cwSymCnt] - 1;
    cwSymTxCompleted = true;
    cwSymCnt++;
  }
  else
  {
    (*cbRfOff)();
    if (cwSymbTab2[cwIndex][cwSymCnt] != 0)
    {
      cwSymTxCompleted = false;
    }
    else
    {   // был последний CW символ буквы, передаём задержку еще два раза. Итого 3 раза (was the last CW character of the letter, we send the delay two more times. Total 3 times)
      cwPassCnt = 2;
      cwSymAllCompleted = true;

      // подготовка к передаче какой нибудь следующей буквы (preparing to send some next letter)
      cwSymTxCompleted = false;
      cwSymCnt = 0;
    }
  }
}


void si_bcn2::process()
{
    if (msgAllCompleted) return;

    if (cwSymAllCompleted && (cwPassCnt == 0))
    {
        msgCnt++;
        cwSymAllCompleted = false;

        if (l_msg[msgCnt] == 0)
        {
          msgAllCompleted = true;
          return;
        }
    }

    cwIndex = getIndex(l_msg[msgCnt]);

    if (cwIndex != 255) processOneSym();
    else 
    {  // пауза между буквами
      cwPassCnt++;
      if (cwPassCnt < 3) return;
      cwPassCnt = 0;
      cwSymAllCompleted = true;
    }
}

bool si_bcn2::isCompleted()
{
  return msgAllCompleted;
}
