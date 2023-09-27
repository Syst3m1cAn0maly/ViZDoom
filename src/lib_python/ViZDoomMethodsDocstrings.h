/*
    This file is autogenerated by scripts/create_python_docstrings_from_cpp_docs.py
    Do not edit it manually. Edit the Markdown files under docs/api/cpp/ directory instead and regenerate it.
*/

#ifndef __VIZDOOM_METHODS_DOCSTRINGS_H__
#define __VIZDOOM_METHODS_DOCSTRINGS_H__

namespace vizdoom {
namespace docstrings {

namespace DoomGamePython {

    const char *init = R"DOCSTRING(Initializes ViZDoom game instance and starts a new episode.
After calling this method, the first state from a new episode will be available.
Some configuration options cannot be changed after calling this method.
Init returns True when the game was started properly and False otherwise.)DOCSTRING";

    const char *close = R"DOCSTRING(Closes ViZDoom game instance.
It is automatically invoked by the destructor.
The game can be initialized again after being closed.)DOCSTRING";

    const char *newEpisode = R"DOCSTRING(Initializes a new episode. The state of an environment is completely restarted (all variables and rewards are reset to their initial values).
After calling this method, the first state from the new episode will be available.
If the `recordingFilePath` is not empty, the new episode will be recorded to this file (as a Doom lump).

In a multiplayer game, the host can call this method to finish the game.
Then the rest of the players must also call this method to start a new episode.)DOCSTRING";

    const char *replayEpisode = R"DOCSTRING(Replays a recorded episode from the given file using the perspective of the specified player.
Players are numbered from 1, `player` equal to 0 results in replaying the demo using the perspective of the default player in the recording file.
After calling this method, the first state from the replay will be available.
All rewards, variables, and states are available during the replaying episode.)DOCSTRING";

    const char *isRunning = R"DOCSTRING(Checks if the ViZDoom game instance is running.)DOCSTRING";

    const char *isMultiplayerGame = R"DOCSTRING(Checks if the game is in multiplayer mode.)DOCSTRING";

    const char *isRecordingEpisode = R"DOCSTRING(Checks if the game is in recording mode.)DOCSTRING";

    const char *isReplayingEpisode = R"DOCSTRING(Checks if the game is in replaying mode.)DOCSTRING";

    const char *setAction = R"DOCSTRING(Sets the player's action for the next tics.
Each value corresponds to a button specified with `addAvailableButton` method
or in the configuration file (in order of appearance).)DOCSTRING";

    const char *advanceAction = R"DOCSTRING(Processes a specified number of tics. If `updateState` is set,
the state will be updated after the last processed tic and a new reward will be calculated.
To get the new state, use `getState` and to get the new reward use `getLastReward`.
If `updateState` is not set, the state will not be updated.)DOCSTRING";

    const char *makeAction = R"DOCSTRING(Method combining usability of `setAction`.
Sets the player's action for the next tics, processes a specified number of tics,
updates the state and calculates a new reward, which is returned.)DOCSTRING";

    const char *isNewEpisode = R"DOCSTRING(Returns True if the current episode is in the initial state - the first state, no actions were performed yet.)DOCSTRING";

    const char *isEpisodeFinished = R"DOCSTRING(Returns True if the current episode is in the terminal state (is finished).
`makeAction`.)DOCSTRING";

    const char *isPlayerDead = R"DOCSTRING(Returns True if the player is dead.
In singleplayer, the player's death is equivalent to the end of the episode.
In multiplayer, when the player is dead `respawnPlayer` method can be called.)DOCSTRING";

    const char *respawnPlayer = R"DOCSTRING(This method respawns the player after death in multiplayer mode.
After calling this method, the first state after the respawn will be available.)DOCSTRING";

    const char *sendGameCommand = R"DOCSTRING(Sends the command to Doom console. It can be used for controlling the game, changing settings, cheats, etc.
Some commands will be blocked in some modes.)DOCSTRING";

    const char *getState = R"DOCSTRING(Returns `GameState` object with the current game state.
If the current episode is finished, `nullptr/null/None` will be returned.)DOCSTRING";

    const char *getServerState = R"DOCSTRING(Returns `ServerState` object with the current server state.)DOCSTRING";

    const char *getLastAction = R"DOCSTRING(Returns the last action performed.
Each value corresponds to a button added with `addAvailableButton.
Most useful in `SPECTATOR` mode.)DOCSTRING";

    const char *getEpisodeTime = R"DOCSTRING(Returns number of current episode tic.)DOCSTRING";

    const char *save = R"DOCSTRING(Saves a game's internal state to the file using ZDoom's save game functionality.)DOCSTRING";

    const char *load = R"DOCSTRING(Loads a game's internal state from the file using ZDoom's load game functionality.
A new state is available after loading.
Loading the game state does not reset the current episode state,
tic counter/time and total reward state keep their values.)DOCSTRING";

    const char *getAvailableButtons = R"DOCSTRING(Returns the list of available `Buttons`.)DOCSTRING";

    const char *setAvailableButtons = R"DOCSTRING(Set given list of `Button`s (e.g. `TURN_LEFT`, `MOVE_FORWARD`) as available `Buttons`.)DOCSTRING";

    const char *addAvailableButton = R"DOCSTRING(Add `Button` to available `Buttons` and sets the maximum allowed, absolute value for the specified button.
If the given button has already been added, it will not be added again, but the maximum value is overridden.)DOCSTRING";

    const char *clearAvailableButtons = R"DOCSTRING(Clears all available `Buttons` added so far.)DOCSTRING";

    const char *getAvailableButtonsSize = R"DOCSTRING(Returns the number of available `Buttons`.)DOCSTRING";

    const char *setButtonMaxValue = R"DOCSTRING(Sets the maximum allowed absolute value for the specified button.
Setting the maximum value to 0 results in no constraint at all (infinity).
This method makes sense only for delta buttons.
The constraints limit applies in all Modes.)DOCSTRING";

    const char *getButtonMaxValue = R"DOCSTRING(Returns the maximum allowed absolute value for the specified button.)DOCSTRING";

    const char *getButton = R"DOCSTRING(Returns the current state of the specified button (`ATTACK`, `USE` etc.).)DOCSTRING";

    const char *getAvailableGameVariables = R"DOCSTRING(Returns the list of available `GameVariables`.)DOCSTRING";

    const char *setAvailableGameVariables = R"DOCSTRING(Set list of `GameVariable` returned by `getState` method.)DOCSTRING";

    const char *addAvailableGameVariable = R"DOCSTRING(Adds the specified `GameVariable` returned by `getState` method.)DOCSTRING";

    const char *clearAvailableGameVariables = R"DOCSTRING(Clears the list of available `GameVariables` that are included in the `GameState` method.)DOCSTRING";

    const char *getAvailableGameVariablesSize = R"DOCSTRING(Returns the number of available `GameVariables`.)DOCSTRING";

    const char *getGameVariable = R"DOCSTRING(Returns the current value of the specified game variable (`HEALTH`, `AMMO1` etc.).
The specified game variable does not need to be among available game variables (included in the state).
It could be used for e.g. shaping. Returns 0 in case of not finding given `GameVariable`.)DOCSTRING";

    const char *addGameArgs = R"DOCSTRING(Adds a custom argument that will be passed to ViZDoom process during initialization.
Useful for changing additional game settings.)DOCSTRING";

    const char *clearGameArgs = R"DOCSTRING(Clears all arguments previously added with `addGameArgs` method.)DOCSTRING";

    const char *getLivingReward = R"DOCSTRING(Returns the reward granted to the player after every tic.)DOCSTRING";

    const char *setLivingReward = R"DOCSTRING(Sets the reward granted to the player after every tic. A negative value is also allowed.

Default value: 0)DOCSTRING";

    const char *getDeathPenalty = R"DOCSTRING(Returns the penalty for the player's death.)DOCSTRING";

    const char *setDeathPenalty = R"DOCSTRING(Sets a penalty for the player's death. Note that in case of a negative value, the player will be rewarded upon dying.

Default value: 0)DOCSTRING";

    const char *getLastReward = R"DOCSTRING(Returns a reward granted after the last update of state.)DOCSTRING";

    const char *getTotalReward = R"DOCSTRING(Returns the sum of all rewards gathered in the current episode.)DOCSTRING";

    const char *loadConfig = R"DOCSTRING(Loads configuration (resolution, available buttons, game variables etc.) from a configuration file.
In case of multiple invocations, older configurations will be overwritten by the recent ones.
Overwriting does not involve resetting to default values. Thus only overlapping parameters will be changed.
The method returns True if the whole configuration file was correctly read and applied,
False if the file contained errors.

If the file relative path is given, it will be searched for in the following order: current directory, current directory + `/scenarios/`, ViZDoom's installation directory + `/scenarios/`.)DOCSTRING";

    const char *getMode = R"DOCSTRING(Returns current mode (`PLAYER`, `SPECTATOR`, `ASYNC_PLAYER`, `ASYNC_SPECTATOR`).)DOCSTRING";

    const char *setMode = R"DOCSTRING(Sets mode (`PLAYER`, `SPECTATOR`, `ASYNC_PLAYER`, `ASYNC_SPECTATOR`) in which the game will be running.

Default value: `PLAYER`.)DOCSTRING";

    const char *getTicrate = R"DOCSTRING(Returns current ticrate.)DOCSTRING";

    const char *setTicrate = R"DOCSTRING(Sets ticrate for ASNYC Modes - number of logic tics executed per second.
The default Doom ticrate is 35. This value will play a game at normal speed.

Default value: 35 (default Doom ticrate).)DOCSTRING";

    const char *setViZDoomPath = R"DOCSTRING(Sets path to the ViZDoom engine executable vizdoom.

Default value: "{vizdoom.so location}/{vizdoom or vizdoom.exe (on Windows)}".)DOCSTRING";

    const char *setDoomGamePath = R"DOCSTRING(Sets the path to the Doom engine based game file (wad format).
If not used DoomGame will look for doom2.wad and freedoom2.wad (in that order) in the directory of ViZDoom's installation (where vizdoom.so/pyd is).

Default value: "{vizdoom.so location}/{doom2.wad, doom.wad, freedoom2.wad or freedoom.wad}")DOCSTRING";

    const char *setDoomScenarioPath = R"DOCSTRING(Sets the path to an additional scenario file (wad format).
If not provided, the default Doom single-player maps will be loaded.

Default value: "")DOCSTRING";

    const char *setDoomMap = R"DOCSTRING(Sets the map name to be used.

Default value: "map01", if set to empty "map01" will be used.)DOCSTRING";

    const char *setDoomSkill = R"DOCSTRING(Sets Doom game difficulty level, which is called skill in Doom.
The higher the skill, the harder the game becomes.
Skill level affects monsters' aggressiveness, monsters' speed, weapon damage, ammunition quantities, etc.
Takes effect from the next episode.

- 1 - VERY EASY, “I'm Too Young to Die” in Doom.
- 2 - EASY, “Hey, Not Too Rough" in Doom.
- 3 - NORMAL, “Hurt Me Plenty” in Doom.
- 4 - HARD, “Ultra-Violence” in Doom.
- 5 - VERY HARD, “Nightmare!” in Doom.

Default value: 3)DOCSTRING";

    const char *setDoomConfigPath = R"DOCSTRING(Sets the path for ZDoom's configuration file.
The file is responsible for the configuration of the ZDoom engine itself.
If it does not exist, it will be created after the `vizdoom` executable is run.
This method is not needed for most of the tasks and is added for the convenience of users with hacking tendencies.

Default value: "", if left empty "_vizdoom.ini" will be used.)DOCSTRING";

    const char *getSeed = R"DOCSTRING(Return ViZDoom's seed.)DOCSTRING";

    const char *setSeed = R"DOCSTRING(Sets the seed of the ViZDoom's RNG that generates seeds (initial state) for episodes.

Default value: randomized in constructor)DOCSTRING";

    const char *getEpisodeStartTime = R"DOCSTRING(Returns start delay of every episode in tics.)DOCSTRING";

    const char *setEpisodeStartTime = R"DOCSTRING(Sets start delay of every episode in tics.
Every episode will effectively start (from the user's perspective) after the provided number of tics.

Default value: 1)DOCSTRING";

    const char *getEpisodeTimeout = R"DOCSTRING(Returns the number of tics after which the episode will be finished.)DOCSTRING";

    const char *setEpisodeTimeout = R"DOCSTRING(Sets the number of tics after which the episode will be finished. 0 will result in no timeout.)DOCSTRING";

    const char *setScreenResolution = R"DOCSTRING(Sets the screen resolution. ZDoom engine supports only specific resolutions.
Supported resolutions are part of ScreenResolution enumeration (e.g., `RES_320X240`, `RES_640X480`, `RES_1920X1080`).
The buffers, as well as the content of ViZDoom's display window, will be affected.

Default value: `RES_320X240`)DOCSTRING";

    const char *getScreenFormat = R"DOCSTRING(Returns the format of the screen buffer and the automap buffer.)DOCSTRING";

    const char *setScreenFormat = R"DOCSTRING(Sets the format of the screen buffer and the automap buffer.
Supported formats are defined in `ScreenFormat` enumeration type (e.g. `CRCGCB`, `RGB24`, `GRAY8`).
The format change affects only the buffers, so it will not have any effect on the content of ViZDoom's display window.

Default value: `CRCGCB`)DOCSTRING";

    const char *isDepthBufferEnabled = R"DOCSTRING(Returns True if the depth buffer is enabled.)DOCSTRING";

    const char *setDepthBufferEnabled = R"DOCSTRING(Enables rendering of the depth buffer, it will be available in the state.
Depth buffer will contain noise if `viz_nocheat` is enabled.

Default value: False)DOCSTRING";

    const char *isLabelsBufferEnabled = R"DOCSTRING(Returns True if the labels buffer is enabled.)DOCSTRING";

    const char *setLabelsBufferEnabled = R"DOCSTRING(Enables rendering of the labels buffer, it will be available in the state with the vector of `Label`s.
LabelsBuffer will contain noise if `viz_nocheat` is enabled.

Default value: False)DOCSTRING";

    const char *isAutomapBufferEnabled = R"DOCSTRING(Returns True if the automap buffer is enabled.)DOCSTRING";

    const char *setAutomapBufferEnabled = R"DOCSTRING(Enables rendering of the automap buffer, it will be available in the state.

Default value: False)DOCSTRING";

    const char *setAutomapMode = R"DOCSTRING(Sets the automap mode (`NORMAL`, `WHOLE`, `OBJECTS`, `OBJECTS_WITH_SIZE`),
which determines what will be visible on it.

Default value: `NORMAL`)DOCSTRING";

    const char *setAutomapRotate = R"DOCSTRING(Determine if the automap will be rotating with the player.
If False, north always will be at the top of the buffer.

Default value: False)DOCSTRING";

    const char *setAutomapRenderTextures = R"DOCSTRING(Determine if the automap will be textured, showing the floor textures.

Default value: True)DOCSTRING";

    const char *setRenderHud = R"DOCSTRING(Determine if the hud will be rendered in the game.

Default value: False)DOCSTRING";

    const char *setRenderMinimalHud = R"DOCSTRING(Determine if the minimalistic version of the hud will be rendered instead of the full hud.

Default value: False)DOCSTRING";

    const char *setRenderWeapon = R"DOCSTRING(Determine if the weapon held by the player will be rendered in the game.

Default value: True)DOCSTRING";

    const char *setRenderCrosshair = R"DOCSTRING(Determine if the crosshair will be rendered in the game.

Default value: False)DOCSTRING";

    const char *setRenderDecals = R"DOCSTRING(Determine if the decals (marks on the walls) will be rendered in the game.

Default value: True)DOCSTRING";

    const char *setRenderParticles = R"DOCSTRING(Determine if the particles will be rendered in the game.

Default value: True)DOCSTRING";

    const char *setRenderEffectsSprites = R"DOCSTRING(Determine if some effects sprites (gun puffs, blood splats etc.) will be rendered in the game.

Default value: True)DOCSTRING";

    const char *setRenderMessages = R"DOCSTRING(Determine if in-game messages (information about pickups, kills, etc.) will be rendered in the game.

Default value: False)DOCSTRING";

    const char *setRenderCorpses = R"DOCSTRING(Determine if actors' corpses will be rendered in the game.

Default value: True)DOCSTRING";

    const char *setRenderScreenFlashes = R"DOCSTRING(Determine if the screen flash effect upon taking damage or picking up items will be rendered in the game.

Default value: True)DOCSTRING";

    const char *setRenderAllFrames = R"DOCSTRING(Determine if all frames between states will be rendered (when skip greater than 1 is used).
Allows smooth preview but can reduce performance.
It only makes sense to use it if the window is visible.

Default value: False)DOCSTRING";

    const char *setWindowVisible = R"DOCSTRING(Determines if ViZDoom's window will be visible.
ViZDoom with window disabled can be used on Linux systems without X Server.

Default value: False)DOCSTRING";

    const char *setConsoleEnabled = R"DOCSTRING(Determines if ViZDoom's console output will be enabled.

Default value: False)DOCSTRING";

    const char *setSoundEnabled = R"DOCSTRING(Determines if ViZDoom's sound will be played.

Default value: False)DOCSTRING";

    const char *getScreenWidth = R"DOCSTRING(Returns game's screen width - width of all buffers.)DOCSTRING";

    const char *getScreenHeight = R"DOCSTRING(Returns game's screen height - height of all buffers.)DOCSTRING";

    const char *getScreenChannels = R"DOCSTRING(Returns number of channels in screen buffer and map buffer (depth and labels buffer always have one channel).)DOCSTRING";

    const char *getScreenPitch = R"DOCSTRING(Returns size in bytes of one row in screen buffer and map buffer.)DOCSTRING";

    const char *getScreenSize = R"DOCSTRING(Returns size in bytes of screen buffer and map buffer.)DOCSTRING";

    const char *isObjectsInfoEnabled = R"DOCSTRING(Returns True if the objects information is enabled.)DOCSTRING";

    const char *setObjectsInfoEnabled = R"DOCSTRING(Enables information about all objects present in the current episode/level.
It will be available in the state.

Default value: False)DOCSTRING";

    const char *isSectorsInfoEnabled = R"DOCSTRING(Returns True if the information about sectors is enabled.)DOCSTRING";

    const char *setSectorsInfoEnabled = R"DOCSTRING(Enables information about all sectors (map layout) present in the current episode/level.
It will be available in the state.

Default value: False)DOCSTRING";

    const char *isAudioBufferEnabled = R"DOCSTRING(Returns True if the audio buffer is enabled.)DOCSTRING";

    const char *setAudioBufferEnabled = R"DOCSTRING(Returns True if the audio buffer is enabled.

Default value: False)DOCSTRING";

    const char *getAudioSamplingRate = R"DOCSTRING(Returns the sampling rate of the audio buffer.)DOCSTRING";

    const char *setAudioSamplingRate = R"DOCSTRING(Sets the sampling rate of the audio buffer.

Default value: False)DOCSTRING";

    const char *getAudioBufferSize = R"DOCSTRING(Returns the size of the audio buffer.)DOCSTRING";

    const char *setAudioBufferSize = R"DOCSTRING(Sets the size of the audio buffer. The size is defined by a number of logic tics.
After each action audio buffer will contain audio from the specified number of the last processed tics.
Doom uses 35 ticks per second.

Default value: 4)DOCSTRING";

} // namespace DoomGamePython

    const char *doomTicsToMs = R"DOCSTRING(Calculates how many tics will be made during given number of milliseconds.)DOCSTRING";

    const char *msToDoomTics = R"DOCSTRING(Calculates the number of milliseconds that will pass during specified number of tics.)DOCSTRING";

    const char *doomTicsToSec = R"DOCSTRING(Calculates how many tics will be made during given number of seconds.)DOCSTRING";

    const char *secToDoomTics = R"DOCSTRING(Calculates the number of seconds that will pass during specified number of tics.)DOCSTRING";

    const char *doomFixedToDouble = R"DOCSTRING(Converts fixed point numeral to a floating point value.
Doom's engine internally use fixed point numbers.
If you read them directly from `USERX` variables,
you may want to convert them to floating point numbers.)DOCSTRING";

    const char *isBinaryButton = R"DOCSTRING(Returns True if button is binary button.)DOCSTRING";

    const char *isDeltaButton = R"DOCSTRING(Returns True if button is delta button.)DOCSTRING";


} // namespace docstrings
} // namespace vizdoom

#endif
