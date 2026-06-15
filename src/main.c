#include <windows.h>

#include "main.h"
#include "Installer.h"
#include "Updater.h"
#include "App.h"

int main(int argc, string* argv) {
    Installer installer = new_Installer();

    if (installer.ensure(&installer, argc, argv))
        return 0;

    App app = new_App();
    app.load(&app);

    Updater updater = new_Updater(installer);
    updater.checkAsync(&updater);

    app.run(&app);

    ExitProcess(0);

    return 0;
}
