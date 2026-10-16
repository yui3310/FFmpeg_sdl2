#include "MainWindow.h"
#include <cstdio>

bool MainWindow::CreateWindow()
{
    int ret = 0;
    
    //init SDL
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);

    Uint32 windowFlag = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    window = SDL_CreateWindow(title, 200, 200, windowWidth, windowHeight, windowFlag);
    if (!window)
    {
        printf("%s\n", SDL_GetError());
        return false;
    }


    return true;
}

bool MainWindow::InitCodec(const char* fileName)
{
    this->fileName = (char*)fileName;

    //open media file
    codec = new Codec(frameQueue, isQuit);
    if (!codec->OpenFile(this->fileName))
        return false;

    if (!codec->GetStreams())
        return false;

    if (!codec->GetCodec())
        return false;

    //create texture
    if (!CreateRenderer())
        return false;

    if (!CreateTexture())
        return false;

    if (!codec->InitAudio())
        return false;

    if (!codec->CreateThread())
        return false;


    return true;
}

bool MainWindow::CreateRenderer()
{
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_TARGETTEXTURE);
    if (!renderer)
    {
        printf("Create Renderer falied\n");
        return false;
    }

    return true;
}


bool MainWindow::CreateTexture()
{
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, 
        codec->videoWidth, codec->videoHeight);

    if (!texture)
    {
        printf("Create Texture falied\n");
        return false;
    }
    
    return true;
}


bool MainWindow::CloseWindow() const
{
    SDL_DestroyRenderer(renderer);
    SDL_DestroyTexture(texture);

    delete codec;

    return true;
}

bool MainWindow::Loop()
{
    SDL_Event event;

    while (!isQuit.load())
    {
        SDL_PollEvent(&event);

        switch (event.type)
        {
        case SDL_QUIT:
            isQuit = 1;
            SDL_DestroyWindow(window);
            break;

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                windowWidth  = event.window.data1;
                windowHeight = event.window.data2;
            }


            break;
        default:
            OnRender();
            SDL_Delay(1);
            break;
        }

    }


    return true;
}

void MainWindow::OnRender()
{
    int ret = -1;

    if (frameQueue.isEmpty())
        return;

    AVFrame* frame = frameQueue.pop();
    if (!frame)
        return;


    //sync
    AVStream* videoStream = codec->GetVideoStream();
    double videoTime = frame->best_effort_timestamp * av_q2d(videoStream->time_base);
    double diff = videoTime - codec->audioTime;
    if (diff > 0.0)
    {
        SDL_Delay((Uint32)(diff * 500));
    }
    else if(diff < -0.1)
    {
        av_frame_free(&frame);
        return;
    }

    //sws
    if (frame->format != AV_PIX_FMT_YUV420P)
    {
        AVFrame* dstFrame = av_frame_alloc();
        codec->sws(frame, dstFrame, frame->width, frame->height);

        //render
        ret = SDL_UpdateYUVTexture(
            texture,
            NULL,
            dstFrame->data[0], dstFrame->linesize[0],
            dstFrame->data[1], dstFrame->linesize[1],
            dstFrame->data[2], dstFrame->linesize[2]);

        av_freep(&dstFrame->data[0]);
        av_frame_free(&dstFrame);
    }
    else
    {
        //render
        ret = SDL_UpdateYUVTexture(
            texture,
            NULL,
            frame->data[0], frame->linesize[0],
            frame->data[1], frame->linesize[1],
            frame->data[2], frame->linesize[2]);
    }


    if (ret < 0)
    {
    	SDL_Log("%s\n", SDL_GetError());
        return;
    }

    //preset
    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
    SDL_RenderClear(renderer);

    //copy
    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = windowWidth;
    rect.h = windowHeight;
    SDL_RenderCopy(renderer, texture, NULL, &rect);

    SDL_RenderPresent(renderer);

    //free
    av_frame_free(&frame);
}

MainWindow::MainWindow(std::atomic<int>& quit) : isQuit(quit)
{
}

MainWindow::~MainWindow()
{
}