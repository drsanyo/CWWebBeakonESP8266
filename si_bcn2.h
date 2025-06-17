
class si_bcn2
{
  char*    l_msg;         // local copy of pointer to message text
  void     (*cbRfOn)();   // address of the callback function for turning on the cw signal
  void     (*cbRfOff)();  // address of the callback function for turning off the cw signal
  
  uint8_t  cwPassCnt;
  uint8_t  cwSymCnt;
  bool     cwSymTxCompleted;
  bool     cwSymAllCompleted;
  uint8_t  cwIndex;

  uint16_t msgCnt;
  bool     msgAllCompleted;
  
  uint8_t  getIndex(char ch);
  void     processOneSym();
  
public:
  void setParams(void (*rfOn)(), void (*rfOff)());   // setting initial parameters
  void setText(char* msg);                           // setting the message text. must be called every time after the completion of the transmission of the previous message before the transmission of the next message
  void process();
  bool isCompleted();
  
  si_bcn2();
};
