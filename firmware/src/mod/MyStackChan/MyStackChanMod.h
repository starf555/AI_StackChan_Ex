#ifndef _MY_STACKCHAN_MOD_H
#define _MY_STACKCHAN_MOD_H

#include <Arduino.h>
#include "mod/ModBase.h"

class MyStackChanMod: public ModBase{
private:
    box_t box_servo;
    box_t box_stt;
    box_t box_BtnA;
    box_t box_BtnC;
    #if defined(ENABLE_CAMERA)
    box_t box_subWindow;
    #endif
    String avatarText;
    bool isOffline;
public:
    MyStackChanMod(bool _isOffline);

    void init(void);
    void pause(void);
    void update(int page_no);
    void btnA_pressed(void);
    void btnB_longPressed(void);
    void btnB_pressed(void);
    void btnC_pressed(void);
    void display_touched(int16_t x, int16_t y);
    void idle(void);
};


#endif  //_MY_STACKCHAN_MOD_H