#include "ViziaDoomGame.h"
#include <iostream>
#include <unistd.h>
#include <vector>

using namespace Vizia;

int main(){

    DoomGame* dg = new DoomGame();

    std::cout << "\n\nVIZIA MAIN EXAMPLE\n\n";

    dg->setDoomGamePath("viziazdoom");
    dg->setDoomIwadPath("../scenarios/doom2.wad");
    dg->setDoomFilePath("../scenarios/s1_b.wad");
    dg->setDoomMap("map01");
    dg->setEpisodeTimeout(200);
    dg->setLivingReward(-1);

    dg->setScreenResolution(640, 480);

    dg->setRenderHud(false);
    dg->setRenderCrosshair(false);
    dg->setRenderWeapon(true);
    dg->setRenderDecals(false);
    dg->setRenderParticles(false);

    dg->setVisibleWindow(true);

    dg->setDisabledConsole(true);

    dg->addAvailableButton(LEFT_RIGHT);
    dg->addAvailableButton(FORWARD_BACKWARD);
    dg->addAvailableButton(VIEW_ANGLE);

    dg->init();
    //dg->newEpisode();
    std::vector<int> action(3);

    action[0] = -5;
    action[1] = 1;
    action[2] = -45;

    int iterations = 10000;
    int ep=1;
    for(int i = 0;i<iterations; ++i){

        if( dg->isEpisodeFinished() ){
            dg->newEpisode();
        }
        DoomGame::State s = dg->getState();

        std::cout << "STATE NUMBER: " << s.number << std::endl;

        dg->setNextAction(action);
        getchar();
        dg->advanceAction();
    }
    dg->close();
    delete dg;
}
