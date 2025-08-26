#include <stdio.h>
#include <windows.h>
#include "util.h"
#include "log.h"

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    util::init();
    logging::Init(/* isOverlay */ true);

    printf("Hello World!\n");
    return 0;
}