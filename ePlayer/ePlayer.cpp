/*
* 
* Need to add FFmpeg_include/ and FFmpeg_lib/ to project
* 
*/

#include "ePlayer.h"
#include <cstdio>

ePlayer::ePlayer()
{
    window = new MainWindow(isQuit);
}

ePlayer& ePlayer::Get()
{
    static ePlayer staticPlayer;
    return staticPlayer;
}

bool ePlayer::Init(const char* fileName)
{
    int ret = -1;

    this->fileName = fileName;

    ret = window->CreateWindow();
    if (!ret)
    {
        printf("Create Window Failed\n");
        return false;
    }

    window->InitCodec(fileName);

    return true;
}

bool ePlayer::OnPlay()
{
    return window->Loop();
}

bool ePlayer::Quit()
{
    window->CloseWindow();
    printf("\nQuit sucessfule\n");
    return true;
}
