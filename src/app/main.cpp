// New Nightmare: Classic Reborn. For now, a viewer of the camera backgrounds,
// to test the window and the renderers (Direct3D 11, Direct3D 12, Vulkan).
//
//   NewNightmare.exe --data "<game folder>" [--renderer d3d11|d3d12|vulkan]
//                    [--level N] [--camera N]
//
// Without --data it uses the folder it is started from if it has the game.
// The game's keys, as the player set them in the game (keys.bin; without it
// its own): arrows walk, turn and walk back (back twice quickly turns round),
// A runs, Space uses (the action of the zone the character is in: examine,
// the trapdoor...; it also answers a script's question, chosen with left and
// right, or closes its message), S turns the flashlight on and off. The
// character comes in where the game puts it, bumps into the walls of the
// level's collision map, and the camera changes by itself when it crosses
// the walls of the trigger map (their scripts ask for it); a script can take
// it to another level.
// The viewer's keys: a click puts the character on the floor there, Page
// Up/Down camera, Home/End level, F3 the background with the flashlight/all
// lit/all dark, C character (Carnby, Aline, none), N/B next/previous
// animation and K next action (idle, walk, run... with the weapon in hand)
// to look at them standing still, H left hand, G right hand or weapon, F
// highlight the foreground, Enter answers too, F2 next renderer, Esc back to the title (on the title: quit).
// The flashlight lights from the left hand; the character must hide behind the
// foreground.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <SDL3/SDL_main.h> // Windows: SDL provides WinMain and calls main

#include "characterBrain.h"
#include "gameControls.h"
#include "gameFont.h"
#include "levelLogic.h"
#include "levelScripts.h"
#include "renderer.h"
#include "audio.h"
#include "userData.h"
#include "soundBanks.h"
#include "actorBrain.h"
#include "npcTable.h"
#include "weaponTable.h"
#include "skeletonModel.h"
#include "viewPicture.h"
#include "viewScene.h"
#include "flashlight.h"
#include "titleMenu.h"
#include "itemFile.h"
#include "saveGame.h"
#include "documentPicture.h"
#include "musicPlayer.h"
#include "movieList.h"
#include "video.h"
#include "radioScene.h"
#include "xaVoices.h"
#include "handLight.h"
#include "particles.h"
#include "window.h"

namespace {

const char* kTitle = "New Nightmare: Classic Reborn";

// Free look (the left Shift or the left mouse button, standing): how far the
// flashlight turns (radians) and how fast with the arrows (radians a second)
constexpr float kLookSideways = 1.2f, kLookUp = 0.5f, kLookDown = 0.8f;
constexpr float kLookSpeed = 1.6f;

// How the background is shown: lit by the flashlight as in the game, all
// lit, all dark
enum class LightMode { Flashlight, Lit, Dark };

const char* lightModeName(LightMode mode)
{
    switch (mode) {
    case LightMode::Flashlight: return "flashlight";
    case LightMode::Lit: return "lit";
    default: return "dark";
    }
}

// A part a character holds at a wrist (a hand, a hand with a weapon), and for
// the right hand the animations of each action with it
struct HeldPart {
    std::string name;
    SkeletonModel model;
    std::vector<std::array<int16_t, 10>> actions;
    std::array<int, 5> soundSlots{ -1, -1, -1, -1, -1 };  // weapons: slots of its bank (loadWeaponSounds)
    HandLight light;  // left hands: its flashlight's flare (HAND chunk)
};

// The actions ("bases") of the character's brain (FUN_00436236), by the
// buttons that lead to them: probable names, not checked in the game
const char* const kActionNames[] = { "idle", "walk", "run", "button 0x80", "aim", "shoot", "turn", "walk aiming",
                                     "shoot walking", "walk back", "walk back aiming", "shoot walking back",
                                     "half-turn", "half-turn aiming", "base 14", "reload?", "base 16" };

// A character the viewer can show, with the hands it can hold
struct Character {
    const char* name;
    const char* file;  // its data in each level: Carnby01, Aline01
    SkeletonModel model;
    std::vector<HeldPart> leftHands, rightHands;
    size_t leftHand = 0, rightHand = 0;  // leftHands.size(), rightHands.size(): none
    std::vector<WeaponCharacteristics> weapons;  // cweap.csv, aweap.csv
};

struct Viewer {
    std::string data;
    std::vector<int> levels;
    size_t levelIndex = 0;
    std::vector<int> cameras;
    size_t cameraIndex = 0;
    LightMode light = LightMode::Flashlight;
    bool shown = false;
    ViewScene scene;
    bool sceneShown = false;
    bool highlight = false;
    bool allActors = false;  // J: every actor made to appear
    std::array<NpcCharacteristics, kNpcKinds> npcs;  // NpcCarac.csv: life of the monsters...
    // Sound: the banks of the game (read from alone4.exe), the samples of the
    // slots of the level's bank (-1: none) and the voice of each handle the
    // scripts were given
    SoundBanks soundBanks;
    std::vector<int> levelSounds;
    std::map<int32_t, int> soundVoices;
    std::map<std::string, int> loadedSounds;  // samples by name (loaded once)
    MusicPlayer music;  // the music and the ambient sounds of the scripts (midi\\)
    size_t soundTried = 0;  // P: the slot played last
    std::vector<Character> characters;
    size_t character = 0;  // characters.size(): none
    // The logic of the level for the chosen character (collision map, where
    // it comes in) and the character led with the keys, in the world
    LevelLogic logic;
    bool logicLoaded = false;
    // The models of the level's bank (of the file the logic came from): the
    // things of the level are drawn with them
    std::vector<SkeletonModel> levelModels;
    // Its scripts, and where the character's last step ended (at the height
    // of the trigger walls) to see which walls the next one crosses
    LevelScripts scripts;
    float stepFrom[3] = {};
    bool stepKnown = false;
    // The zones of the trigger map the character is in, in the order it went
    // in, and the one whose action the action button does (-1: none)
    std::vector<bool> inZone;
    std::vector<int> zonesIn;
    int actionZone = -1;
    int fromLevel = -1;     // the level left last: it chooses the way in
    int chosenAnswer = 0;   // of the question of a script on the screen
    GameFont font;
    // The screens before playing (title, choosing the hero) and whether the
    // game's menu files were found
    TitleMenu menu;
    bool menuLoaded = false;
    std::map<int, ItemInfo> items;  // the texts of the items, read once (item\IT3D_N)
    std::string notice;             // what the game says ("You take..."), until closed
    // The full-screen picture a script shows (functions 8, 63): its name
    // once on the screen, and since when
    std::string documentShown;
    uint64_t documentSince = 0;
    // The videos (MOVIES.TXT), the ones still to play in turn (number, and
    // whether a key may skip it), what comes after them, and the one playing
    std::map<int, std::string> movies;
    std::vector<std::pair<int, bool>> movieQueue;
    enum class AfterMovies { Nothing, Title, ChooseHero, Script, NewGame } afterMovies = AfterMovies::Nothing;
    bool moviePlaying = false;
    uint64_t movieSince = 0;
    std::vector<uint32_t> movieFrame;  // 640 x 480, the picture in the middle
    // The hero led by the scripts or a scene of the radio (its state 3,
    // FUN_00437d10): the buttons held for it, an animation to play to its end
    // (an identifier), a marked place to walk to (passed when the way from
    // where it started turns back) and a turn to face one
    bool heroLed = false;
    uint32_t ledButtons = 0;
    int ledAnimation = -1;
    int walkMarker = -1;
    float walkFrom[2] = {};
    bool turning = false;
    float turnFrom = 0, turnTo = 0, turnTime = 0, turnDuration = 0;
    // The scenes of the radio (radio\N) and their voices (xa\, Xa.txt): the
    // sample of each voice once read, and the one playing
    RadioScene radio;
    XaVoices voices;
    std::map<int, int> voiceSounds;
    int voiceHandle = -1;
    uint64_t menuShownAt = 0;    // when its screen was shown (it fades in)
    bool menuClicking = false;   // the left button was held the frame before
    bool useBlocked = false;      // the use key still held from a menu: ignored until let go
    float menuMouse[2] = { -1, -1 };  // where the mouse was last still (on the desktop)
    GameControls gameKeys = defaultGameControls();  // the player's keys (keys.bin)
    bool flashlightOn = true;  // the game's flashlight (its key turns it on and off)
    // Free look: how far the flashlight is turned from where the character
    // looks (radians: to its left, up), and the mouse when it started
    float lookYaw = 0, lookPitch = 0;
    bool mouseLooking = false;
    float mouseFrom[2] = {};
    // The beam, in the world: from the flashlight in the left hand
    bool beamOn = false;
    float beamPlace[3] = {}, beamAim[3] = {};
    // The flare of the flashlight seen from the camera (FUN_00414fa9), and
    // the hand whose pictures the renderer has
    std::vector<OverlayRect> flareRects;
    const HandLight* flareHand = nullptr;
    // The particle systems of the level (its bank), an emitter for each thing
    // of the level with how long it is shown (+0x34 of the game's object:
    // -1 always, 0 not), the rain (its emitter, on, and where its drops are
    // let out: a square in front of the camera, FUN_00415e9c) and the flash
    // of lightning over the scene (FUN_00423059)
    std::vector<ParticleSystem> particleSystems;
    std::vector<ParticleEmitter> thingEmitters;
    std::vector<int> thingShown;
    ParticleEmitter rain;
    bool rainOn = false, rainFresh = true;
    float rainBox[3] = {};
    int flashLeft = 0;
    float flashStrength = 0;
    // Where each system's texture is in the glows' image (pixels: x, y)
    std::vector<std::array<int, 2>> systemPlaces;
    // The aiming screen (FUN_0046d920): the bullets in the clip of each weapon
    // (ours: no reloading yet, a clip fills again when it is empty) and the
    // beat of the life bar (FUN_00414422)
    std::map<int, int> clip;
    int lifeBeat = 0;
    // The left hand held before an animation took the radio out (event 0x66
    // with 2, FUN_004497bd; -1: none)
    int savedLeftHand = -1;
    size_t missingShown = 0;  // functions of the scripts not done, in the title
    CharacterMover mover;
    CharacterControls controls;
    bool placed = false;
    // Or an animation looked at standing still (N, B, K) until the arrows
    // are used again
    bool still = false;
    size_t animation = 0;  // of the character
    size_t action = 0;     // the last chosen with K (kActionNames)
    bool byAction = false; // the animation was chosen by action
    uint64_t animationStart = 0;  // milliseconds() when it started
};

int currentLevel(const Viewer& v)
{
    return v.levels[v.levelIndex];
}

// Puts in the title what is shown: level, camera, light, character and animation
void showTitle(const Viewer& v)
{
    const int camera = v.cameras.empty() ? 0 : v.cameras[v.cameraIndex];
    char who[160] = "";
    if (v.character < v.characters.size()) {
        const SkeletonModel& model = v.characters[v.character].model;
        const Character& c = v.characters[v.character];
        auto partName = [](const std::vector<HeldPart>& parts, size_t chosen) {
            if (chosen >= parts.size())
                return std::string("none");
            const std::string& file = parts[chosen].name;
            return file.substr(file.find_last_of('/') + 1);
        };
        char action[48] = "";
        int animation = int(v.animation);
        if (!v.still) {
            std::snprintf(action, sizeof(action), " (%s%s)", characterStateName(v.mover.state),
                          v.heroLed ? ", led by the game" : "");
            animation = v.mover.animation;
        } else if (v.byAction)
            std::snprintf(action, sizeof(action), " (%s)", kActionNames[v.action]);
        std::snprintf(who, sizeof(who), " - %s, life %d, animation %d of %zu%s, %s + %s", c.name,
                      v.scripts.state().life, animation, model.animations.size(), action,
                      partName(c.leftHands, c.leftHand).c_str(), partName(c.rightHands, c.rightHand).c_str());
    }
    char scripts[64] = "";
    if (!v.scripts.missingFunctions().empty())
        std::snprintf(scripts, sizeof(scripts), " [scripts: %zu functions not done]",
                      v.scripts.missingFunctions().size());
    const size_t sounds = size_t(std::count_if(v.levelSounds.begin(), v.levelSounds.end(), [](int s) { return s >= 0; }));
    char title[512];
    std::snprintf(title, sizeof(title), "%s - level %d, camera %d (%s, %zu sounds)%s%s%s - %s, sound: %s", kTitle,
                  currentLevel(v), camera, lightModeName(v.light), sounds, who, v.shown ? "" : " [cannot be read]",
                  scripts, rendererName(), audioOutputName().c_str());
    setWindowTitle(title);
}

// Loads both pictures of the current camera (dark and lit) and updates the title
void showCamera(Viewer& v)
{
    ViewPicture dark, lit;
    const int camera = v.cameras.empty() ? 0 : v.cameras[v.cameraIndex];
    v.scripts.setShownCamera(camera);
    v.shown =!v.cameras.empty() && loadViewPicture(v.data, currentLevel(v), camera, false, dark)
              && loadViewPicture(v.data, currentLevel(v), camera, true, lit);
    if (v.shown)
        setPictures(kViewWidth, kViewHeight, dark.rgba.data(), lit.rgba.data());

    // The scene and the foreground
    ViewSheet darkSheet;
    v.sceneShown = v.shown && loadViewScene(v.data, currentLevel(v), camera, v.scene)
                   && loadViewSheet(v.data, currentLevel(v), camera, false, darkSheet);
    if (v.sceneShown) {
        std::vector<MeshVertex> points;
        for (const ScenePoint& p : v.scene.points)
            points.push_back({ p.x, p.y, p.z, p.u, p.v });
        const std::vector<uint8_t> mask = foregroundMask(darkSheet);
        size_t roomCount = 0;
        const std::vector<uint32_t> triangles = sceneTriangles(v.scene, &roomCount);
        setForegroundMesh(points, triangles, roomCount, darkSheet.width, darkSheet.height, mask.data());
        setCameraFocal(v.scene.focal);
    } else {
        setForegroundMesh({}, {}, 0, 0, 0, nullptr);
        setCameraFocal(0.0f);
    }
    showTitle(v);
}

// A model and its vertices in a pose, to be drawn together with others: how
// it is turned and where it is in the world, and its first texture among the
// ones given to the renderer (showViewerTextures)
struct PosedPiece {
    const SkeletonModel* model;
    std::vector<ModelVertex> vertices;
    float turn[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };  // rows
    float place[3] = {};
    int firstTexture = 0;
    bool shadow = false;  // casts its shadow on the floor at shadowFloor
    float shadowFloor = 0;
};

// A turn about z by an angle (more is to the left, as the character's heading)
void turnAboutZ(float angle, float out[9])
{
    const float c = std::cos(angle), s = std::sin(angle);
    const float m[9] = { c, -s, 0, s, c, 0, 0, 0, 1 };
    std::copy(m, m + 9, out);
}

// The turn of a thing of the level by its angles: about z, then y, then x
// (FUN_00476ea9)
void turnByAngles(const float angles[3], float out[9])
{
    const float cx = std::cos(angles[0]), sx = std::sin(angles[0]);
    const float cy = std::cos(angles[1]), sy = std::sin(angles[1]);
    const float cz = std::cos(angles[2]), sz = std::sin(angles[2]);
    // Rz * Ry * Rx
    const float m[9] = { cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx,
                         sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx,
                         -sy,     cy * sx,                cy * cx };
    std::copy(m, m + 9, out);
}

// The pieces of a character to draw: its body with its skeleton in a pose and
// the parts it holds at its wrists
std::vector<PosedPiece> poseCharacter(const Character& c, const std::vector<BoneMatrix>& bones)
{
    std::vector<PosedPiece> pieces(1);
    pieces[0].model = &c.model;
    skinVertices(c.model, bones, pieces[0].vertices);
    const struct {
        const std::vector<HeldPart>& parts;
        size_t chosen;
        bool left;
    } hands[] = { { c.leftHands, c.leftHand, true }, { c.rightHands, c.rightHand, false } };
    for (const auto& hand : hands) {
        const int wrist = wristBone(c.model, hand.left);
        if (hand.chosen >= hand.parts.size() || wrist < 0 || size_t(wrist) >= bones.size())
            continue;
        PosedPiece piece{ &hand.parts[hand.chosen].model, {} };
        attachVertices(*piece.model, bones[size_t(wrist)], piece.vertices);
        pieces.push_back(std::move(piece));
    }
    return pieces;
}

// Places the pieces in the world and gives them to the renderer as one model,
// seen from the camera of the scene. The models are in centimetres with z up
// and look along +y
// The shadows of the pieces that cast one (the hero, the actors): as the
// game's (FUN_00423dc0) from the scene's light, high up (the moon; the second
// one when the level is lit), but flat: every triangle of the piece put on
// the floor under its feet along the light's way (ours; the game extrudes a
// shadow mesh of each model)
std::vector<float> floorShadows(const std::vector<PosedPiece>& pieces, const ViewScene& scene, int light)
{
    std::vector<float> out;
    const float* l = scene.lights[light].place;
    for (const PosedPiece& piece : pieces) {
        if (!piece.shadow)
            continue;
        const float* m = piece.turn;
        std::vector<float> flat(piece.vertices.size() * 2);
        std::vector<bool> seen(piece.vertices.size());
        for (size_t k = 0; k < piece.vertices.size(); k++) {
            const ModelVertex& v = piece.vertices[k];
            const float world[3] = { m[0] * v.x + m[1] * v.y + m[2] * v.z + piece.place[0],
                                     m[3] * v.x + m[4] * v.y + m[5] * v.z + piece.place[1],
                                     m[6] * v.x + m[7] * v.y + m[8] * v.z + piece.place[2] };
            const float above = l[2] - world[2];
            if (above <= 1.0f)
                continue;
            const float along = (l[2] - piece.shadowFloor) / above;
            const float onFloor[3] = { l[0] + (world[0] - l[0]) * along, l[1] + (world[1] - l[1]) * along,
                                       piece.shadowFloor + 0.5f };
            float c[3];
            worldToCamera(scene, onFloor, c);
            if (c[1] <= 1.0f)
                continue;
            projectToPicture(scene, c, flat[2 * k], flat[2 * k + 1]);
            seen[k] = true;
        }
        for (const ModelMaterial& material : piece.model->materials)
            for (size_t k = 0; k + 2 < material.triangles.size(); k += 3) {
                const uint32_t a = material.triangles[k], b = material.triangles[k + 1], c = material.triangles[k + 2];
                if (a >= seen.size() || b >= seen.size() || c >= seen.size() || !seen[a] || !seen[b] || !seen[c])
                    continue;
                out.insert(out.end(), { flat[2 * a], flat[2 * a + 1], flat[2 * b], flat[2 * b + 1], flat[2 * c], flat[2 * c + 1] });
            }
    }
    return out;
}

void placePieces(const std::vector<PosedPiece>& pieces, const ViewScene& scene, int light = 0)
{
    setShadows(floorShadows(pieces, scene, light), 0.55f);
    std::vector<ModelPoint> points;
    std::vector<ModelPart> parts;
    for (const PosedPiece& piece : pieces) {
        const float* m = piece.turn;
        const uint32_t firstPoint = uint32_t(points.size());
        for (const ModelVertex& v : piece.vertices) {
            const float world[3] = { m[0] * v.x + m[1] * v.y + m[2] * v.z + piece.place[0],
                                     m[3] * v.x + m[4] * v.y + m[5] * v.z + piece.place[1],
                                     m[6] * v.x + m[7] * v.y + m[8] * v.z + piece.place[2] };
            const float turned[3] = { m[0] * v.nx + m[1] * v.ny + m[2] * v.nz, m[3] * v.nx + m[4] * v.ny + m[5] * v.nz,
                                      m[6] * v.nx + m[7] * v.ny + m[8] * v.nz };
            float place[3], normal[3];
            worldToCamera(scene, world, place);
            worldDirectionToCamera(scene, turned, normal);
            points.push_back({ place[0], place[1], place[2], normal[0], normal[1], normal[2], v.u, v.v });
        }
        for (const ModelMaterial& material : piece.model->materials) {
            ModelPart part{ material.texture >= 0 ? piece.firstTexture + material.texture : -1, material.triangles };
            for (uint32_t& index : part.triangles)
                index += firstPoint;
            parts.push_back(std::move(part));
        }
    }
    setModel(points, parts);
}

// The models whose textures the renderer has, in order: the chosen
// character's (body, left hand, right hand, as poseCharacter gives them) and
// then every model of the level's bank
std::vector<const SkeletonModel*> viewerModels(const Viewer& v)
{
    std::vector<const SkeletonModel*> models;
    if (v.character < v.characters.size()) {
        const Character& c = v.characters[v.character];
        models.push_back(&c.model);
        if (c.leftHand < c.leftHands.size() && wristBone(c.model, true) >= 0)
            models.push_back(&c.leftHands[c.leftHand].model);
        if (c.rightHand < c.rightHands.size() && wristBone(c.model, false) >= 0)
            models.push_back(&c.rightHands[c.rightHand].model);
    }
    for (const SkeletonModel& model : v.levelModels)
        models.push_back(&model);
    return models;
}

// Where the textures of a model start among the ones the renderer has
int firstTextureOf(const Viewer& v, const SkeletonModel* model)
{
    int first = 0;
    for (const SkeletonModel* m : viewerModels(v)) {
        if (m == model)
            return first;
        first += int(m->textures.size());
    }
    return 0;
}

// Gives the renderer the textures of the character, its hands and the
// level's models (viewerModels)
void showViewerTextures(const Viewer& v)
{
    std::vector<ModelImage> images;
    for (const SkeletonModel* model : viewerModels(v))
        for (const ModelTexture& texture : model->textures)
            images.push_back({ texture.size, texture.rgba });
    setModelTextures(images);
}

// The things of the level that are shown and have a model (FUN_0046850c),
// where the level puts them: playing their animation over and over while they
// are animated (flag 0x08, function 40), else in their rest pose
void poseLevelObjects(const Viewer& v, std::vector<PosedPiece>& out)
{
    if (!v.logicLoaded)
        return;
    for (const LevelObject& object : v.logic.objects) {
        if (!object.visible || (object.flags & kObjectHasModel) == 0 || object.model < 0
            || size_t(object.model) >= v.levelModels.size())
            continue;
        const SkeletonModel& model = v.levelModels[size_t(object.model)];
        std::vector<BoneMatrix> bones;
        size_t animation = model.animations.size();  // the rest pose
        float time = 0;
        if (object.animating && object.animation >= 0 && size_t(object.animation) < model.animations.size()) {
            animation = size_t(object.animation);
            const uint32_t length = model.animations[animation].length;
            time = length > 0 ? float(milliseconds() % length) : 0.0f;
        }
        poseSkeleton(model, animation, time, bones);
        PosedPiece piece{ &model, {} };
        skinVertices(model, bones, piece.vertices);
        turnByAngles(object.angles, piece.turn);
        std::copy(object.place, object.place + 3, piece.place);
        piece.firstTexture = firstTextureOf(v, &model);
        out.push_back(std::move(piece));
    }
}

// Plays a slot of the level's sound bank made by something at a place of
// the world (FUN_00418c9a): louder on the side of the screen it is on (the
// other side down to 60 of 127), and lower away from the camera past 4 m
// (down to 8). nullptr: everywhere at full volume
void playSampleAt(Viewer& v, int sample, const float* place, int note = 60, int least = 0)
{
    if (sample < 0)
        return;
    int left = 127, right = 127;
    if (place && v.sceneShown) {
        float inCamera[3];
        worldToCamera(v.scene, place, inCamera);
        if (inCamera[1] > 1.0f) {
            float x, y;
            projectToPicture(v.scene, inCamera, x, y);
            if (x <= 320)
                right = std::clamp(int(x * 127 / 320), 60, 127);
            else
                left = std::clamp(int((640 - x) * 127 / 320), 60, 127);
        }
        const float distance = std::sqrt(inCamera[0] * inCamera[0] + inCamera[1] * inCamera[1] + inCamera[2] * inCamera[2]);
        if (distance > 400) {
            const float d = std::min(distance, 1500.0f);
            left = std::max(int(left - (d - 400) * left * 2 / 3300), 8);
            right = std::max(int(right - (d - 400) * right * 2 / 3300), 8);
        }
    }
    left = std::max(left, least);
    right = std::max(right, least);
    const int sum = left + right;
    playSound(sample, float(sum) / 2.0f / 127.0f, float(right) / float(sum), std::pow(2.0f, float(note - 60) / 12.0f));
}

void playLevelSound(Viewer& v, int slot, const float* place)
{
    if (slot >= 0 && size_t(slot) < v.levelSounds.size())
        playSampleAt(v, v.levelSounds[size_t(slot)], place);
}

std::vector<int> bankSamples(Viewer& v, const std::string& file);

// A sound of the weapon in the right hand: one of its slots (+0xC..+0x10 of
// its WEAP chunk) in its own bank, as the note 64 and at least 50 of 127 a
// side (FUN_00469cd4, FUN_0046cda4)
void playWeaponSound(Viewer& v, int which)
{
    if (v.character >= v.characters.size())
        return;
    const Character& c = v.characters[v.character];
    if (c.rightHand >= c.rightHands.size() || which < 0 || which >= 5)
        return;
    const HeldPart& part = c.rightHands[c.rightHand];
    const int slot = part.soundSlots[size_t(which)];
    if (slot < 0)
        return;
    std::string bank = part.name + ".";  // "weapon/carnby/weapon0." as the game's "weapon\carnby\weapon0."
    for (char& ch : bank)
        if (ch == '/')
            ch = '\\';
    const std::vector<int> samples = bankSamples(v, bank);
    if (size_t(slot) < samples.size()) {
        const float at[3] = { v.mover.x, v.mover.y, v.mover.z + 100 };
        playSampleAt(v, samples[size_t(slot)], at, 64, 50);
    }
}

// The left hand of a number (lefthand\<hero>\handN), or the one held if none
size_t leftHandNumber(const Character& c, int number)
{
    const std::string end = "hand" + std::to_string(number);
    for (size_t k = 0; k < c.leftHands.size(); k++) {
        const std::string& name = c.leftHands[k].name;
        if (name.size() >= end.size() && name.compare(name.size() - end.size(), end.size(), end) == 0)
            return k;
    }
    return c.leftHand;
}

// The moments of the hero's animation that change its left hand
// (FUN_00434409, code 0x66): with 2 the hand held is kept and the radio
// (hand 2) taken (FUN_004497bd, FUN_004493c7); else the hand kept comes back,
// its light as the flashlight's switch says (FUN_004497d4, FUN_0044970b)
void heroHandEvents(Viewer& v)
{
    if (v.character >= v.characters.size())
        return;
    Character& c = v.characters[v.character];
    for (const AnimationEvent& e : v.mover.events) {
        if (e.code != 0x66)
            continue;
        if (e.value == 2) {
            v.savedLeftHand = int(c.leftHand);
            c.leftHand = leftHandNumber(c, 2);
        } else if (v.savedLeftHand >= 0) {
            c.leftHand = size_t(v.savedLeftHand);
            v.savedLeftHand = -1;
        }
        showTitle(v);
    }
}

// The sounds of the moments of the hero's animation passed this frame
// (FUN_00434409): code 100 a footstep, the slot of its value in the table of
// the floor under its feet; code 0x65 with 100, 0x67 or 0x6E the general
// sounds 4, 5 and 9 (its weapon's sounds, 1 to 3, are not done)
void heroSounds(Viewer& v)
{
    if (!v.logicLoaded)
        return;
    const float at[3] = { v.mover.x, v.mover.y, v.mover.z + 100 };
    for (const AnimationEvent& e : v.mover.events) {
        int slot = -1;
        if (e.code == 100 && v.mover.floorKind >= 0 && size_t(v.mover.floorKind) < v.logic.floorSounds.size()
            && e.value < 72)
            slot = v.logic.floorSounds[size_t(v.mover.floorKind)][e.value];
        else if (e.code == 0x65) {
            const int general = e.value == 100 ? 4 : e.value == 0x67 ? 5 : e.value == 0x6E ? 9 : -1;
            if (general >= 0)
                slot = v.logic.generalSounds[size_t(general)];
            // The weapon's: 1 its +0xD, 2 its +0x10 if it has no +0xE, 3 its +0x10
            if (v.character < v.characters.size() && v.characters[v.character].rightHand < v.characters[v.character].rightHands.size()) {
                const HeldPart& part = v.characters[v.character].rightHands[v.characters[v.character].rightHand];
                if (e.value == 1)
                    playWeaponSound(v, 1);
                else if ((e.value == 2 && part.soundSlots[2] < 0) || e.value == 3)
                    playWeaponSound(v, 4);
            }
        }
        playLevelSound(v, slot, at);
    }
    heroHandEvents(v);
}

// The animation (identifier) of the hero hurt by a kind of actor
// (0x4C5F00, 16 kinds a row; the row by where the blow comes from: 0, 1 or 2;
// three more rows, not used here, when FUN_00434197 says so)
int hurtAnimation(int kind, int side)
{
    static const int16_t table[3][16] = {
        { 14, 14, 17, 16, 15, 501, 14, 14, 16, 14, 130, 14, 14, 15, 130, 130 },
        { 14, 14, 23, 16, 15, 501, 14, 14, 16, 14, 130, 14, 14, 15, 130, 130 },
        { 14, 14, 19, 16, 15, 501, 14, 14, 16, 14, 130, 14, 14, 15, 130, 130 },
    };
    if (kind < 0 || kind >= 16 || side < 0 || side > 2)
        return 14;
    return table[side][kind];
}

// A blow of an actor reaches the hero (FUN_00435281, FUN_00435338): it loses
// life and is hurt, with the animation of the blow by the actor's kind and
// where it comes from (case 0x11 of FUN_00436236), or it dies
void hurtHero(Viewer& v, const LevelActor& by, int damage)
{
    int& life = v.scripts.state().life;
    life = std::max(life - damage, 0);
    // From the hero to the actor, against where the hero looks (0x800 of
    // 4096: half): ahead 2, behind 0, at a side 1
    const float dx = v.mover.x - by.body.x, dy = v.mover.y - by.body.y;
    const float d = std::sqrt(dx * dx + dy * dy);
    const float along = d > 0.01f ? (-std::sin(v.mover.heading) * dx + std::cos(v.mover.heading) * dy) / d : 0.0f;
    const int side = along > 0.5f ? 2 : along < -0.5f ? 0 : 1;
    hitCharacter(v.mover, hurtAnimation(by.kind, side), life < 1);
    showTitle(v);
}

// One frame of the brains of the actors that are here or dying, against
// the hero (the character led, or far away if there is none)
void updateActors(Viewer& v, float frameMilliseconds)
{
    if (!v.logicLoaded)
        return;
    const bool hero = v.character < v.characters.size() && v.placed && !v.still;
    const float heroAt[3] = { hero ? v.mover.x : 1e9f, hero ? v.mover.y : 1e9f, hero ? v.mover.z : 1e9f };
    const bool heroAlive = hero && v.mover.state != CharacterState::Dead;
    for (LevelActor& actor : v.logic.actors) {
        if ((actor.presence != ActorPresence::Here && actor.presence != ActorPresence::Dying) || actor.model < 0
            || size_t(actor.model) >= v.levelModels.size())
            continue;
        const NpcCharacteristics* npc =
            actor.kind >= 0 && actor.kind < kNpcKinds ? &v.npcs[size_t(actor.kind)] : nullptr;
        std::vector<int> sounds;
        // The beam on it (the game: its model got the flashlight's light,
        // flag 2 of +0x15C, FUN_00418ab3): at its middle, a metre up
        const float middle[3] = { actor.body.x, actor.body.y, actor.body.z + 100 };
        const bool inLight = hero && v.beamOn && flashlightStrength(v.beamPlace, v.beamAim, middle) > 0.0f;
        const int damage = updateActor(actor, v.levelModels[size_t(actor.model)], heroAt, heroAlive, frameMilliseconds,
                                       v.logic.hasCollision ? &v.logic.collision : nullptr, npc, &sounds, inLight);
        const float at[3] = { actor.body.x, actor.body.y, actor.body.z + 100 };
        for (int sound : sounds)
            if (actor.kind >= 0 && actor.kind < 16 && sound >= 0 && sound < 24)
                playLevelSound(v, v.logic.actorSounds[size_t(actor.kind)][size_t(sound)], at);
        if (damage > 0 && heroAlive && v.mover.state != CharacterState::Dead)
            hurtHero(v, actor, damage);
    }
}

void loadLogic(Viewer& v);
void placeCharacterAtStart(Viewer& v, bool keepCamera);

// The hero has lain dead a while: the level starts again (its actors and
// things as at first) with full life. The game ends the game instead
// (FUN_0044d444), not done
void restartAfterDeath(Viewer& v)
{
    if (v.mover.state != CharacterState::Dead || v.mover.sinceDeath < 3000)
        return;
    v.scripts.state().life = 100;
    v.mover = CharacterMover();
    loadLogic(v);
    placeCharacterAtStart(v, false);
    showTitle(v);
}

// The actors of the level that are drawn, where their
// bodies are, in the pose their brains left (or standing in the first
// animation of their model, if their brains have not run)
void poseLevelActors(const Viewer& v, std::vector<PosedPiece>& out)
{
    if (!v.logicLoaded)
        return;
    for (const LevelActor& actor : v.logic.actors) {
        if (!actor.shown || actor.model < 0 || size_t(actor.model) >= v.levelModels.size())
            continue;
        const SkeletonModel& model = v.levelModels[size_t(actor.model)];
        std::vector<BoneMatrix> bones;
        if (!actor.body.pose.place.empty())
            buildSkeleton(model, actor.body.pose, bones);
        else {
            size_t animation = model.animations.size();  // the rest pose
            float time = 0;
            if (!model.animations.empty() && model.animations[0].length > 0) {
                animation = 0;
                time = float(milliseconds() % model.animations[0].length);
            }
            poseSkeleton(model, animation, time, bones);
        }
        PosedPiece piece{ &model, {} };
        skinVertices(model, bones, piece.vertices);
        turnAboutZ(actor.body.heading, piece.turn);
        piece.place[0] = actor.body.x;
        piece.place[1] = actor.body.y;
        piece.place[2] = actor.body.z;
        piece.firstTexture = firstTextureOf(v, &model);
        piece.shadow = true;
        piece.shadowFloor = actor.body.z;
        out.push_back(std::move(piece));
    }
}

// Puts the character on the floor seen at a point of the 640x480 picture,
// keeping its heading, and onto the floor of the collision map there. False
// if there is no floor there
bool placeCharacter(Viewer& v, float pictureX, float pictureY)
{
    float onFloor[3], world[3];
    if (!v.sceneShown || !pickFloor(v.scene, pictureX, pictureY, onFloor))
        return false;
    cameraToWorld(v.scene, onFloor, world);
    v.mover.x = world[0];
    v.mover.y = world[1];
    v.mover.z = world[2];
    if (v.logic.hasCollision)
        standOnFloor(v.mover, v.logic.collision);
    v.placed = true;
    v.stepKnown = false;  // put there, not walked: no trigger walls crossed
    return true;
}

// Shows a camera of the level by its number, if it has it and it is not the
// one shown
void showCameraNumber(Viewer& v, int number)
{
    const auto found = std::find(v.cameras.begin(), v.cameras.end(), number);
    if (found == v.cameras.end() || size_t(found - v.cameras.begin()) == v.cameraIndex)
        return;
    v.cameraIndex = size_t(found - v.cameras.begin());
    showCamera(v);
}

// Puts the character where it comes into the level, as the game does
// (FUN_0044b91b): by the way in from the level left last (FUN_0044b3e1), with
// the camera of that way, and runs the level's script and the way's (which
// may choose another camera). With keepCamera (a camera asked for, a
// character changed) the camera stays and the way's script is not run. If the
// level does not say, on the floor in the middle of the picture (or lower)
void placeCharacterAtStart(Viewer& v, bool keepCamera)
{
    v.placed = false;
    v.stepKnown = false;
    const LevelEntry* entry = v.logicLoaded ? chooseEntry(v.logic, v.fromLevel) : nullptr;
    if (entry) {
        v.mover.x = entry->place[0];
        v.mover.y = entry->place[1];
        v.mover.z = entry->place[2];
        v.mover.heading = entry->heading;
        if (v.logic.hasCollision)
            standOnFloor(v.mover, v.logic.collision);
        v.placed = true;
        if (!keepCamera)
            showCameraNumber(v, entry->camera);
        v.scripts.startScript(v.logic.levelScript);
        if (!keepCamera)
            v.scripts.startScript(entry->script);
        return;
    }
    for (float y : { 0.5f, 0.7f, 0.85f, 0.3f })
        if (placeCharacter(v, 0.5f * kViewWidth, y * kViewHeight))
            return;
}

// A sample of SOUND2 by its name (loaded the first time), -1 if none
int sampleNamed(Viewer& v, const std::string& name)
{
    if (name.empty())
        return -1;
    const auto known = v.loadedSounds.find(name);
    if (known != v.loadedSounds.end())
        return known->second;
    const int sound = loadSound((std::filesystem::path(v.data) / "Sound2" / (name + ".wav")).string());
    v.loadedSounds[name] = sound;
    return sound;
}

// The samples of the slots of the sound bank of a data file (its name as
// the game's table has it), -1 where there is none
std::vector<int> bankSamples(Viewer& v, const std::string& file)
{
    std::vector<int> samples;
    const auto bank = v.soundBanks.files.find(file);
    if (bank != v.soundBanks.files.end())
        for (const std::string& name : bank->second)
            samples.push_back(sampleNamed(v, name));
    return samples;
}

// Loads the samples of the sound bank of a level's file (the slots that
// function 45 of the scripts plays): SOUND2\<name>.wav
void loadLevelSounds(Viewer& v, const char* file)
{
    stopAllVoices();
    v.soundVoices.clear();
    v.levelSounds = bankSamples(v, levelSoundBankName(currentLevel(v), file));
}

// Plays what the scripts asked for this frame (functions 45, 47, 48): a slot
// as a note (60 its own pitch, each step a semitone), with the volumes left
// and right made a volume and a pan as the game does (FUN_0049a393)
// What the scripts asked of the music and the ambient sounds (functions 93,
// 94, 51, 52, 53, 95), done on the music player
void playScriptMusic(Viewer& v)
{
    for (const LevelScripts::MusicRequest& r : v.scripts.takeMusicRequests()) {
        switch (r.kind) {
        case LevelScripts::MusicRequest::Load:
            v.music.loadFile(v.data, r.name);
            break;
        case LevelScripts::MusicRequest::Play:
            v.music.play(r.channel, r.name, r.a, r.b, r.c, r.d);
            break;
        case LevelScripts::MusicRequest::Section:
            v.music.setSection(r.channel, r.a, r.b);
            break;
        case LevelScripts::MusicRequest::Stop:
            v.music.stop(r.channel);
            break;
        case LevelScripts::MusicRequest::Volume:
            v.music.setVolume(r.channel, r.a, r.b);
            break;
        case LevelScripts::MusicRequest::Fade:
            v.music.fade(r.channel, r.a, r.b, r.c);
            break;
        }
    }
}

void playScriptSounds(Viewer& v)
{
    playScriptMusic(v);
    for (const LevelScripts::SoundRequest& r : v.scripts.takeSoundRequests()) {
        switch (r.kind) {
        case LevelScripts::SoundRequest::Play: {
            const auto before = v.soundVoices.find(r.handle);  // the channel plays the new one
            if (before != v.soundVoices.end())
                stopVoice(before->second);
            const int sound = r.slot >= 0 && size_t(r.slot) < v.levelSounds.size() ? v.levelSounds[size_t(r.slot)] : -1;
            const int sum = std::max(r.left, 0) + std::max(r.right, 0);
            const float volume = std::min(float(sum) / 2.0f / 127.0f, 1.0f);
            const float pan = sum > 0 ? float(std::max(r.right, 0)) / float(sum) : 0.5f;
            const float pitch = std::pow(2.0f, float(r.note - 60) / 12.0f);
            v.soundVoices[r.handle] = playSound(sound, volume, pan, pitch);
            break;
        }
        case LevelScripts::SoundRequest::Stop: {
            const auto voice = v.soundVoices.find(r.handle);
            if (voice != v.soundVoices.end())
                stopVoice(voice->second);
            break;
        }
        case LevelScripts::SoundRequest::StopAll:
            stopAllVoices();
            break;
        }
    }
}

// Loads the logic of the level for the chosen character (or Carnby's, with
// none chosen; or the other one's if the level has no file for it)
// A number by chance from 0 to 1
float chance01()
{
    return float(std::rand()) / float(RAND_MAX);
}

// The bullets of the aiming screen of each weapon of both heroes ("BALLES" of
// weapon\<hero>\weaponN, FUN_0041619a), put in a free part of the menus'
// image (from 1600, 600; a row a hero)
void loadBulletPictures(Viewer& v)
{
    if (!v.menuLoaded)
        return;
    const char* heroes[] = { "carnby", "aline" };
    for (int h = 0; h < 2; h++)
        for (int w = 0; w < 10; w++) {
            FlarePicture picture;
            const std::string file = std::string("weapon/") + heroes[h] + "/weapon" + std::to_string(w);
            if (!loadNamedTexture(v.data, file, "BALLES", picture))
                continue;
            const int x = 1600 + 40 * w, y = 600 + 40 * h;
            for (int py = 0; py < picture.height; py++)
                for (int px = 0; px < picture.width; px++)
                    v.menu.atlas[size_t(y + py) * size_t(v.menu.atlasWidth) + size_t(x + px)] =
                        picture.rgba[size_t(py) * size_t(picture.width) + size_t(px)];
            v.menu.placed.push_back({ std::string("BALLES_") + heroes[h] + std::to_string(w), x, y, picture.width, picture.height });
        }
}

// The particle systems of a level's file and what uses them: an emitter for
// each thing of the level (shown always with its flag 0x01, FUN_0044f5b5) and
// the rain, if the level has it (flag 0x02, its system at +0x3F1; on from the
// start with flag 0x04; FUN_0044a39d)
void loadLevelParticles(Viewer& v, const char* file)
{
    loadParticleSystems(v.data, currentLevel(v), file, v.particleSystems);
    v.thingEmitters.clear();
    v.thingShown.clear();
    for (const LevelThing& thing : v.logic.things) {
        const ParticleSystem* system = thing.system >= 0 && size_t(thing.system) < v.particleSystems.size()
                                           ? &v.particleSystems[size_t(thing.system)]
                                           : nullptr;
        ParticleEmitter emitter(system);
        std::memcpy(emitter.place, thing.place, sizeof(thing.place));
        emitter.turn = thing.angles[2];
        v.thingEmitters.push_back(std::move(emitter));
        v.thingShown.push_back((thing.flags & 1) ? -1 : 0);
    }
    const int rain = v.logic.rainSystem;
    v.rain = ParticleEmitter((v.logic.flags & 2) && rain >= 0 && size_t(rain) < v.particleSystems.size()
                                 ? &v.particleSystems[size_t(rain)]
                                 : nullptr);
    v.rainOn = (v.logic.flags & 4) != 0;
    v.rainFresh = true;
    v.flashLeft = 0;
    v.flareHand = nullptr;  // the glows' image is made again with these textures
}

void loadLogic(Viewer& v)
{
    std::vector<const char*> files;
    if (v.character < v.characters.size())
        files.push_back(v.characters[v.character].file);
    files.push_back("Carnby01");
    files.push_back("Aline01");
    v.logicLoaded = false;
    v.levelModels.clear();
    for (const char* file : files)
        if ((v.logicLoaded = loadLevelLogic(v.data, currentLevel(v), file, v.logic))) {
            loadSkeletonModels(v.data, currentLevel(v), file, v.levelModels);
            loadLevelParticles(v, file);
            loadLevelSounds(v, file);
            break;
        }
    showViewerTextures(v);
    v.scripts.setLevel(v.logicLoaded ? &v.logic : nullptr, currentLevel(v));
    v.scripts.setWeapons(v.character < v.characters.size() ? &v.characters[v.character].weapons : nullptr);
    v.stepKnown = false;
}

// Goes to another level as a script asked (function 4, FUN_0044dd12): the
// character comes in by the way from the level it leaves
void leadHero(Viewer& v, bool led);
int actionVariation(const Character& c, bool flashlightOn);

void goToLevel(Viewer& v, int level)
{
    const auto found = std::find(v.levels.begin(), v.levels.end(), level);
    if (found == v.levels.end())
        return;
    v.fromLevel = currentLevel(v);
    v.levelIndex = size_t(found - v.levels.begin());
    v.radio.stop();
    leadHero(v, false);
    setCinemaBands(0);
    v.cameras = cameraNumbers(v.data, currentLevel(v));
    v.cameraIndex = 0;
    showCamera(v);
    loadLogic(v);
    placeCharacterAtStart(v, false);
}

// The character went into (or out of) zone k of the trigger map: its script
// for that runs, and the zone whose action the action button does is the
// last one gone into that has one (FUN_00466f84, FUN_00467031)
void changeZone(Viewer& v, int k, bool in)
{
    const ZoneTrigger& zone = v.logic.zoneTriggers[size_t(k)];
    v.scripts.startScript(in ? zone.enterScript : zone.leaveScript);
    if (in)
        v.zonesIn.push_back(k);
    else
        v.zonesIn.erase(std::remove(v.zonesIn.begin(), v.zonesIn.end(), k), v.zonesIn.end());
    v.actionZone = -1;
    for (int z : v.zonesIn)
        if (v.logic.zoneTriggers[size_t(z)].actionScript != 0)
            v.actionZone = z;
}

// The action button (FUN_00449cb7): the action of the zone the character is
// in, or the level's for none. A zone with flag 0x02 only if the character
// faces its way: the angle of its forward axis on the floor (atan2 of y and
// x, degrees) minus the zone's must be within its margin (the game does not
// wrap the difference round)
void doAction(Viewer& v)
{
    if (!v.placed || v.still || v.character >= v.characters.size() || v.scripts.question() || v.scripts.message())
        return;
    if (v.actionZone < 0) {
        v.scripts.startScript(v.logic.noActionScript);
        return;
    }
    const ZoneTrigger& zone = v.logic.zoneTriggers[size_t(v.actionZone)];
    if (zone.flags & 0x02) {
        constexpr float kDegrees = 180.0f / 3.14159265f;
        const float forwardX = -std::sin(v.mover.heading), forwardY = std::cos(v.mover.heading);
        const float facing = std::atan2(forwardY, forwardX) * kDegrees;
        if (std::fabs(facing - float(zone.faceAngle)) > float(zone.faceMargin))
            return;
    }
    v.scripts.startScript(zone.actionScript);
}

// The level's part of a frame, after the character moved: the trigger walls
// its step crossed start their scripts (FUN_004a33fb, FUN_00466ee8), the
// scripts run, and the camera they ask for is shown
// The hero led by the scripts (true) or by the player again (FUN_00437d10,
// FUN_00436037): nothing held, nothing asked
void leadHero(Viewer& v, bool led)
{
    v.heroLed = led;
    v.ledButtons = 0;
    v.ledAnimation = -1;
    v.walkMarker = -1;
    v.turning = false;
}

// What the orders of a scene of the radio do, for the viewer
class ViewerRadio : public RadioActions {
public:
    explicit ViewerRadio(Viewer& v) : v_(v) {}
    void requestCamera(int camera) override { showCameraNumber(v_, camera); }
    int shownCamera() const override { return v_.cameras.empty() ? -1 : v_.cameras[v_.cameraIndex]; }
    void leadHero(bool byScene) override { ::leadHero(v_, byScene); }
    void holdHeroButtons(uint32_t buttons) override
    {
        v_.ledButtons = buttons;
        v_.ledAnimation = -1;
    }
    void walkHeroTo(int marker) override
    {
        v_.walkMarker = marker >= 0 && size_t(marker) < v_.logic.markers.size() ? marker : -1;
        v_.walkFrom[0] = v_.mover.x;
        v_.walkFrom[1] = v_.mover.y;
    }
    bool heroWalking() const override { return v_.walkMarker >= 0; }
    void turnHeroTo(int marker, int milliseconds) override
    {
        if (marker < 0 || size_t(marker) >= v_.logic.markers.size())
            return;
        const LevelMarker& m = v_.logic.markers[size_t(marker)];
        // Facing the place: at heading 0 the hero looks along +y, more is left
        const float target = std::atan2(-(m.place[0] - v_.mover.x), m.place[1] - v_.mover.y);
        float by = std::remainder(target - v_.mover.heading, 2.0f * 3.14159265f);
        v_.turnFrom = v_.mover.heading;
        v_.turnTo = v_.mover.heading + by;
        v_.turnTime = 0;
        v_.turnDuration = float(milliseconds);
        v_.turning = milliseconds > 0;
        if (!v_.turning)
            v_.mover.heading = v_.turnTo;
    }
    bool heroTurning() const override { return v_.turning; }
    void playHeroAnimation(int identifier, bool, bool) override { v_.ledAnimation = identifier; }
    bool heroAnimationPlaying() const override { return v_.ledAnimation >= 0; }
    void playVoice(int voice) override
    {
        auto found = v_.voiceSounds.find(voice);
        if (found == v_.voiceSounds.end()) {
            std::vector<int16_t> samples;
            int rate = 0;
            const int sound = v_.voices.read(voice, samples, rate)
                                  ? loadSoundSamples(samples.data(), int(samples.size()), rate)
                                  : -1;
            found = v_.voiceSounds.emplace(voice, sound).first;
        }
        if (v_.voiceHandle >= 0)
            stopVoice(v_.voiceHandle);
        v_.voiceHandle = found->second >= 0 ? playSound(found->second, 1.0f, 0.5f, 1.0f) : -1;
    }
    bool voicePlaying() const override { return v_.voiceHandle >= 0 && ::voicePlaying(v_.voiceHandle); }
    void giveItem(int item) override
    {
        takeItem(v_.scripts.state().inventory, item, 1, v_.scripts.weapons());
        v_.scripts.notePickup(item, 1);
    }
    void setFlashlight(bool on) override
    {
        // Only with a flashlight in the left hand (FUN_00435751, FUN_00449791)
        if (v_.character < v_.characters.size() && actionVariation(v_.characters[v_.character], true) == 1)
            v_.flashlightOn = on;
    }
    void putHeroAt(int marker) override { v_.scripts.requestMarker(marker); }
    void setCinemaBands(bool on) override { ::setCinemaBands(on ? 40.0f : 0.0f); }
    void showThing(int thing, int milliseconds) override
    {
        if (thing >= 0 && size_t(thing) < v_.thingShown.size())
            v_.thingShown[size_t(thing)] = milliseconds;
    }

private:
    Viewer& v_;
};

// The scene of the radio a script asked for, and a frame of the one playing;
// when it is over, the numbered script it was given starts (FUN_0044da0a)
void stepRadio(Viewer& v, float frameMilliseconds)
{
    ViewerRadio actions(v);
    LevelScripts::RadioRequest asked = v.scripts.takeRadioRequest();
    if (!asked.name.empty()) {
        std::string name = asked.name;
        while (!name.empty() && (name.front() == '\\' || name.front() == '/'))
            name.erase(name.begin());
        if (!v.radio.start((std::filesystem::path(v.data) / name).string(), asked.script, actions)) {
            if (asked.script >= 0)
                v.scripts.startEvent(asked.script);
        }
        return;
    }
    int after = -1;
    const bool was = v.radio.running();
    v.radio.step(frameMilliseconds, actions, after);
    if (was && !v.radio.running()) {
        if (after >= 0)
            v.scripts.startEvent(after);
        showTitle(v);
    }
}

// The square in front of the camera where the rain's drops are let out
// (FUN_00415e9c: 500 ahead of the camera, 300 to each side). The game puts it
// at a height of 430 (levels whose floor is near 0); ours: 130 over the
// camera, the same in every level
void rainSquare(const Viewer& v, float out[3])
{
    const float ahead[3] = { 0, 1, 0 };
    float forward[3];
    cameraToWorld(v.scene, ahead, forward);
    for (int k = 0; k < 3; k++)
        forward[k] -= v.scene.position[k];
    out[0] = v.scene.position[0] + forward[0] * 500.0f - 300.0f;
    out[1] = v.scene.position[1] + forward[1] * 500.0f - 300.0f;
    out[2] = v.scene.position[2] + 130.0f;
}

// A frame of the effects (FUN_004227df): what the scripts asked for, the
// things shown (FUN_0044f695), the particles of every emitter, the rain's
// new drops (FUN_00415dd7: one every 10 milliseconds somewhere in its
// square), and the flash
void stepEffects(Viewer& v, int milliseconds)
{
    for (const LevelScripts::EffectRequest& r : v.scripts.takeEffectRequests()) {
        if (r.kind == LevelScripts::EffectRequest::ShowThing) {
            if (r.a >= 0 && size_t(r.a) < v.thingShown.size())
                v.thingShown[size_t(r.a)] = r.b;
        } else if (r.kind == LevelScripts::EffectRequest::Rain) {
            if (v.logic.flags & 2)
                v.rainOn = r.a != 0;
        } else {
            v.flashStrength = float(r.a) / 255.0f;
            v.flashLeft = 100;
        }
    }
    for (size_t k = 0; k < v.thingEmitters.size(); k++) {
        int& shown = v.thingShown[k];
        if (shown != 0) {
            if (shown > 0)
                shown = std::max(0, shown - milliseconds);
            v.thingEmitters[k].keepEmitting();
        }
        v.thingEmitters[k].step(milliseconds);
    }
    if (v.rainOn && v.rain.system() && v.sceneShown) {
        float box[3];
        rainSquare(v, box);
        if (v.rainFresh) {
            // At first (and at a new camera): two lives of rain at once
            v.rain.stop();
            for (int t = 0; t < 2 * v.rain.system()->life; t += 10) {
                v.rain.place[0] = box[0] + chance01() * 600.0f;
                v.rain.place[1] = box[1] + chance01() * 600.0f;
                v.rain.place[2] = box[2];
                v.rain.emit(10);
                v.rain.step(10);
            }
            v.rainFresh = false;
        } else if (box[0] != v.rainBox[0] || box[1] != v.rainBox[1] || box[2] != v.rainBox[2]) {
            // A new camera: the drops come along (FUN_00415e9c)
            for (Particle& p : v.rain.particles())
                if (v.rain.alive(p))
                    for (int c = 0; c < 3; c++)
                        p.place[c] += box[c] - v.rainBox[c];
        }
        std::memcpy(v.rainBox, box, sizeof(box));
        for (int k = 0; k < milliseconds / 10; k++) {
            v.rain.place[0] = box[0] + chance01() * 600.0f;
            v.rain.place[1] = box[1] + chance01() * 600.0f;
            v.rain.place[2] = box[2];
            v.rain.emit(10);
        }
        v.rain.step(milliseconds);
    }
    v.flashLeft = std::max(0, v.flashLeft - milliseconds);
}

void stepLevel(Viewer& v, float frameMilliseconds)
{
    if (v.placed && !v.still && v.character < v.characters.size()) {
        const float now[3] = { v.mover.x, v.mover.y, v.mover.z + kTriggerHeight };
        if (v.stepKnown && (now[0] != v.stepFrom[0] || now[1] != v.stepFrom[1] || now[2] != v.stepFrom[2])) {
            std::vector<TriggerCrossing> crossed;
            crossedWalls(v.logic.triggers, v.stepFrom, now, crossed);
            const int camera = v.cameras.empty() ? -1 : v.cameras[v.cameraIndex];
            for (const TriggerCrossing& crossing : crossed) {
                if (size_t(crossing.wall) >= v.logic.passTriggers.size())
                    continue;
                const PassTrigger& trigger = v.logic.passTriggers[size_t(crossing.wall)];
                if (trigger.camera >= 0 && trigger.camera != camera)
                    continue;
                v.scripts.startScript(crossing.entering ? trigger.enterScript : trigger.leaveScript);
            }
        }
        // Zones: in and out (just put there: from out of all)
        if (!v.stepKnown) {
            v.inZone.assign(v.logic.zoneTriggers.size(), false);
            v.zonesIn.clear();
            v.actionZone = -1;
        }
        for (size_t k = 0; k < v.inZone.size() && k < v.logic.triggers.zones.size(); k++) {
            const bool in = insideZone(v.logic.triggers.zones[k], now);
            if (in != v.inZone[k]) {
                v.inZone[k] = in;
                changeZone(v, int(k), in);
            }
        }
        std::memcpy(v.stepFrom, now, sizeof(now));
        v.stepKnown = true;
    }
    v.scripts.runFrame(frameMilliseconds);
    for (const LevelScripts::HeroRequest& r : v.scripts.takeHeroRequests()) {
        if (r.kind == LevelScripts::HeroRequest::Buttons) {
            v.ledButtons = r.buttons;
            v.ledAnimation = -1;
        } else
            leadHero(v, r.kind == LevelScripts::HeroRequest::Lead);
    }
    stepRadio(v, frameMilliseconds);
    stepEffects(v, int(frameMilliseconds));
    const int camera = v.scripts.takeCameraRequest();
    if (camera >= 0)
        showCameraNumber(v, camera);
    const int marker = v.scripts.takeMarkerRequest();
    if (marker >= 0 && size_t(marker) < v.logic.markers.size()) {
        // Function 72: the character at a marked place of the level
        const LevelMarker& place = v.logic.markers[size_t(marker)];
        v.mover.x = place.place[0];
        v.mover.y = place.place[1];
        v.mover.z = place.place[2];
        v.mover.heading = place.angles[2];
        if (v.logic.hasCollision)
            standOnFloor(v.mover, v.logic.collision);
        v.stepKnown = false;
    }
    const int level = v.scripts.takeLevelRequest();
    if (level >= 0)
        goToLevel(v, level);
    if (v.scripts.missingFunctions().size() != v.missingShown) {
        v.missingShown = v.scripts.missingFunctions().size();
        showTitle(v);
    }
}

// Adds a line of text to the layer over everything, its letters of the game's
// font scale times their size, from a place of the 640x480 picture
void addText(std::vector<OverlayRect>& out, const GameFont& font, const std::string& text, float x, float y,
             float scale, uint32_t colour)
{
    const float size = kGlyphSize * scale;
    for (unsigned char letter : text) {
        OverlayRect r{ x, y, size, size, 0, 0, 0, 0, colour, true };
        if (glyphPlace(font, letter, r.u0, r.v0, r.u1, r.v1))
            out.push_back(r);
        x += size;
    }
}

// Splits a text into lines of at most that many letters, at spaces
std::vector<std::string> wrapText(const std::string& text, size_t width)
{
    std::vector<std::string> lines;
    std::string line;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find(' ', start);
        if (end == std::string::npos)
            end = text.size();
        const std::string word = text.substr(start, end - start);
        if (!line.empty() && line.size() + 1 + word.size() > width) {
            lines.push_back(line);
            line.clear();
        }
        line += (line.empty() ? "" : " ") + word;
        start = end + 1;
    }
    if (!line.empty())
        lines.push_back(line);
    return lines;
}

// What the layer over everything shows: the question of a script, in a box at
// the bottom, with its answers (the chosen one in yellow), or its message. The
// game draws its own box (FUN_0043ce5f, FUN_0043d661, not studied); this one
// is the viewer's
const WeaponCharacteristics* heldWeapon(const Character& c);

// The aiming screen, while the aim key is held with a weapon (FUN_0043a655):
// the bullets in the clip from x 32, 59 pixels over the bottom, one picture
// each (FUN_004161ed), and the life bar: a beat going over a 146 x 8 band
// at the bottom right, fading behind, faster and of another colour as life
// goes down (FUN_0041439b, FUN_004144c3)
void addAimingScreen(Viewer& v, std::vector<OverlayRect>& rects, int milliseconds)
{
    const int life = v.scripts.state().life;
    const int tier = life < 26 ? 3 : life < 76 ? 2 : life < 100 ? 1 : 0;
    const int speed = tier == 3 ? 4 : tier == 2 ? 3 : 2;
    v.lifeBeat += speed * milliseconds;
    if (v.character >= v.characters.size() || !v.menuLoaded)
        return;
    const Character& c = v.characters[v.character];
    const WeaponCharacteristics* weapon = heldWeapon(c);
    if (!weapon || weapon->number < 0 || !keyHeld(v.gameKeys[kActionAim]) || v.heroLed)
        return;
    // The bullets
    const std::string name = std::string("BALLES_") + (c.file[0] == 'A' ? "aline" : "carnby") + std::to_string(weapon->number);
    for (const TitleMenu::Placed& p : v.menu.placed) {
        if (p.name != name)
            continue;
        auto found = v.clip.find(weapon->number);
        const int bullets = found != v.clip.end() ? found->second : weapon->shotsPerClip;
        const float u0 = float(p.x) / float(v.menu.atlasWidth), u1 = float(p.x + p.width) / float(v.menu.atlasWidth);
        const float v0 = float(p.y) / float(v.menu.atlasHeight), v1 = float(p.y + p.height) / float(v.menu.atlasHeight);
        for (int k = 0; k < std::min(bullets, 40); k++)
            rects.push_back({ 32.0f + float(k * p.width), 421.0f, float(p.width), float(p.height), u0, v0, u1, v1, 0xFFFFFFFFu, true });
    }
    // The life bar: its head goes from 0 to 292, a tail of 3/4 of 146 behind
    constexpr float kColours[4][3] = { { 35, 188, 215 }, { 0, 255, 0 }, { 255, 128, 0 }, { 255, 0, 0 } };
    const int head = (v.lifeBeat / 33) % 292, tail = head - 146 * 3 / 4;
    constexpr int kPieces = 16;
    for (int k = 0; k < kPieces; k++) {
        const float from = float(tail) + float(head - tail) * float(k) / kPieces;
        const float to = float(tail) + float(head - tail) * float(k + 1) / kPieces;
        const float a = std::max(from, 0.0f), b = std::min(to, 146.0f);
        if (b <= a)
            continue;
        const float bright = (float(k) + 0.5f) / kPieces;
        const uint32_t colour = uint32_t(kColours[tier][0] * bright) | uint32_t(kColours[tier][1] * bright) << 8
                                | uint32_t(kColours[tier][2] * bright) << 16 | 0xFF000000u;
        rects.push_back({ 442.0f + a, 438.0f, b - a, 8.0f, 0, 0, 0, 0, colour, false });
    }
}

void showQuestion(Viewer& v)
{
    const LevelScripts::Question* question = v.scripts.question();
    const std::string* message = v.scripts.message();
    if (!question && !message && !v.notice.empty())
        message = &v.notice;
    std::vector<OverlayRect> rects;
    if ((question || message) && v.menuLoaded) {
        rects = textBoxRects(v.menu, question ? question->text : *message,
                             question ? question->answers : std::vector<std::string>(), v.chosenAnswer);
    } else if ((question || message) && !v.font.rgba.empty()) {
        constexpr float kScale = 2.0f, kLine = kGlyphSize * kScale + 6.0f;
        const std::vector<std::string> lines = wrapText(question ? question->text : *message, 36);
        const float height = kLine * float(lines.size() + (question ? 1 : 0)) + 24.0f, top = 470.0f - height;
        rects.push_back({ 20, top, 600, height, 0, 0, 0, 0, 0xC0000000u, false });
        float y = top + 12.0f;
        for (const std::string& line : lines) {
            addText(rects, v.font, line, 36, y, kScale, 0xFFFFFFFFu);
            y += kLine;
        }
        float x = 36;
        for (size_t k = 0; question && k < question->answers.size(); k++) {
            const bool chosen = int(k) == v.chosenAnswer;
            addText(rects, v.font, question->answers[k], x, y, kScale, chosen ? 0xFF00FFFFu : 0xFF909090u);
            x += kGlyphSize * kScale * float(question->answers[k].size() + 3);
        }
    }
    const uint64_t now = milliseconds();
    static uint64_t before = now;
    addAimingScreen(v, rects, int(std::min<uint64_t>(now - before, 100)));
    before = now;
    setOverlay(rects);
}

// The actions of the weapon in the right hand. The empty hand (righthand) has
// none: the ones of righthd are used. nullptr if there are none
const std::vector<std::array<int16_t, 10>>* rightHandActions(const Character& c)
{
    if (c.rightHand < c.rightHands.size() && !c.rightHands[c.rightHand].actions.empty())
        return &c.rightHands[c.rightHand].actions;
    for (const HeldPart& part : c.rightHands)
        if (part.name.find("righthd") != std::string::npos && !part.actions.empty())
            return &part.actions;
    return nullptr;
}

// The weapon in the right hand (weapon\<hero>\weaponN), or nullptr if
// it holds none
const WeaponCharacteristics* heldWeapon(const Character& c)
{
    if (c.rightHand >= c.rightHands.size())
        return nullptr;
    const std::string& name = c.rightHands[c.rightHand].name;
    const size_t at = name.rfind("/weapon");
    if (at == std::string::npos)
        return nullptr;
    const int number = std::atoi(name.c_str() + at + 7);
    for (const WeaponCharacteristics& w : c.weapons)
        if (w.number == number)
            return &w;
    static const WeaponCharacteristics unknown;  // a weapon not in the table
    return &unknown;
}

// How far the hero shoots and how wide it looks for a target (not read from
// the game yet)
constexpr float kShotRange = 2000.0f, kShotCone = 0.5f;

// While aiming with a weapon that locks onto targets, the hero turns towards
// the nearest actor ahead (FUN_00438920 looks for one)
void lockOn(Viewer& v, const WeaponCharacteristics& weapon, float frameMilliseconds)
{
    if (!weapon.autolock || !aimingState(v.mover.state) || !v.logicLoaded)
        return;
    const int target = aimedActor(v.logic.actors, v.mover, kShotRange, kShotCone);
    if (target < 0)
        return;
    const LevelActor& a = v.logic.actors[size_t(target)];
    const float wanted = std::atan2(-(a.body.x - v.mover.x), a.body.y - v.mover.y);
    const float left = std::remainder(wanted - v.mover.heading, 2.0f * 3.14159265f);
    const float part = std::min(1.0f, frameMilliseconds * 0.01f);
    v.mover.heading += left * part;
}

// A shot leaves the weapon: it hits the actor aimed at, if there is one
void shoot(Viewer& v, const WeaponCharacteristics& weapon)
{
    if (!v.logicLoaded)
        return;
    const int target = aimedActor(v.logic.actors, v.mover, kShotRange, kShotCone);
    if (target < 0)
        return;
    LevelActor& a = v.logic.actors[size_t(target)];
    const NpcCharacteristics* npc = a.kind >= 0 && a.kind < kNpcKinds ? &v.npcs[size_t(a.kind)] : nullptr;
    hurtActor(a, weapon.damage, npc);
}

// The variation of the actions (FUN_0043a286): 1 with the flashlight on and
// in the left hand (hand1, hand3, hand4)
int actionVariation(const Character& c, bool flashlightOn)
{
    if (!flashlightOn || c.leftHand >= c.leftHands.size())
        return 0;
    const char last = c.leftHands[c.leftHand].name.back();
    return last == '1' || last == '3' || last == '4' ? 1 : 0;
}

// Gives the renderer the light of this frame: the flashlight's beam (in
// camera space) and the light everywhere, by the viewer's light mode
std::vector<GlowQuad> particleQuads(const Viewer& v);
void useFlareHand(Viewer& v, const HandLight& light);

void showLight(Viewer& v)
{
    setLight(v.light == LightMode::Lit ? 1.0f : 0.0f);
    float place[3] = {}, aim[3] = { 0, 1, 0 };
    const bool on = v.beamOn && v.sceneShown && v.light == LightMode::Flashlight && v.character < v.characters.size();
    if (on) {
        worldToCamera(v.scene, v.beamPlace, place);
        worldDirectionToCamera(v.scene, v.beamAim, aim);
    }
    setFlashlight(on, place, aim, kFlashlightReach);
    setGlows(on ? v.flareRects : std::vector<OverlayRect>());
    // The glows' image, with the textures of the level's particle systems
    static const HandLight noLight;
    const Character* hero = v.character < v.characters.size() ? &v.characters[v.character] : nullptr;
    useFlareHand(v, hero && hero->leftHand < hero->leftHands.size() ? hero->leftHands[hero->leftHand].light : noLight);
    // A flash of lightning lights the whole scene a moment (FUN_00423059)
    if (v.flashLeft > 0 && v.light == LightMode::Flashlight)
        setLight(v.flashStrength);
    setGlowQuads(particleQuads(v));
}

// The places of the flare's pictures in the image of the glows: FLARE and
// GLOW side by side, LINE under them, with black between them (blending the
// nearest pixels, the edge of one would take a line of the next); the
// textures of the level's particle systems after them, row by row
constexpr int kGlowWidth = 1024, kGlowHeight = 1024, kGlowLineTop = 72, kGlowGlowLeft = 72, kGlowSystemsTop = 96;

// Gives the renderer the pictures of a hand's flare, once
void useFlareHand(Viewer& v, const HandLight& light)
{
    if (v.flareHand == &light && v.systemPlaces.size() == v.particleSystems.size())
        return;
    v.flareHand = &light;
    std::vector<uint32_t> image(size_t(kGlowWidth) * kGlowHeight, 0xFF000000u);
    auto put = [&](const FlarePicture& picture, int left, int top) {
        for (int y = 0; y < picture.height && top + y < kGlowHeight; y++)
            for (int x = 0; x < picture.width && left + x < kGlowWidth; x++)
                image[size_t(top + y) * kGlowWidth + size_t(left + x)] = picture.rgba[size_t(y) * size_t(picture.width) + size_t(x)];
    };
    put(light.flare, 0, 0);
    put(light.glow, kGlowGlowLeft, 0);
    put(light.line, 0, kGlowLineTop);
    // The systems' textures, a gap of 8 around each
    v.systemPlaces.assign(v.particleSystems.size(), { -1, -1 });
    int x = 0, y = kGlowSystemsTop, row = 0;
    for (size_t k = 0; k < v.particleSystems.size(); k++) {
        const FlarePicture& texture = v.particleSystems[k].texture;
        if (texture.rgba.empty())
            continue;
        if (x + texture.width > kGlowWidth) {
            x = 0;
            y += row + 8;
            row = 0;
        }
        if (y + texture.height > kGlowHeight)
            break;
        put(texture, x, y);
        v.systemPlaces[k] = { x, y };
        x += texture.width + 8;
        row = std::max(row, texture.height);
    }
    setGlowImage(kGlowWidth, kGlowHeight, image.data());
}

// The particles to draw this frame, as the game draws them (FUN_0041e6df):
// sprites turned by their angle, their size focal x size / 8192 / depth;
// flat squares on the ground; streaks (rain) from where they are back along
// their way, a pixel wide, grey at a quarter (its way of blending 3)
std::vector<GlowQuad> particleQuads(const Viewer& v)
{
    std::vector<GlowQuad> out;
    if (!v.sceneShown || v.systemPlaces.size() != v.particleSystems.size())
        return out;
    const float focal = v.scene.focal;
    auto project = [&](const float world[3], float& x, float& y, float& depth) {
        float c[3];
        worldToCamera(v.scene, world, c);
        depth = c[1];
        if (depth <= 1.0f)
            return false;
        projectToPicture(v.scene, c, x, y);
        return true;
    };
    auto draw = [&](const ParticleEmitter& emitter) {
        const ParticleSystem* s = emitter.system();
        if (!s)
            return;
        const size_t index = size_t(s - v.particleSystems.data());
        if (index >= v.systemPlaces.size() || v.systemPlaces[index][0] < 0)
            return;
        const float ax = float(v.systemPlaces[index][0]), ay = float(v.systemPlaces[index][1]);
        const float tw = float(s->texture.width), th = float(s->texture.height);
        for (const Particle& p : emitter.particles()) {
            if (!emitter.drawn(p))
                continue;
            const int blend = s->shape == ParticleShape::Streak ? 3 : p.blend;
            const float strength = blend == 0 ? 0.5f : blend == 3 ? 0.25f : 1.0f;
            const float grey = s->shape == ParticleShape::Streak ? 0.5f : 1.0f;
            auto part = [&](uint8_t c) { return uint32_t(std::clamp(float(c) * grey * strength, 0.0f, 255.0f)); };
            GlowQuad q;
            q.colour = part(p.colour[0]) | part(p.colour[1]) << 8 | part(p.colour[2]) << 16 | 0xFF000000u;
            const float us[4] = { p.u0, p.u1, p.u0, p.u1 }, vs[4] = { p.v0, p.v0, p.v1, p.v1 };
            for (int k = 0; k < 4; k++) {
                q.u[k] = (ax + us[k] * tw) / float(kGlowWidth);
                q.v[k] = (ay + vs[k] * th) / float(kGlowHeight);
            }
            float x, y, depth;
            if (s->shape == ParticleShape::Sprite) {
                if (!project(p.place, x, y, depth))
                    continue;
                const float half = focal * p.size / 8192.0f / depth;
                const float cs = std::cos(p.angle) * half, sn = std::sin(p.angle) * half;
                const float dx[4] = { -cs + sn, cs + sn, -cs - sn, cs - sn }, dy[4] = { -sn - cs, sn - cs, -sn + cs, sn + cs };
                for (int k = 0; k < 4; k++) {
                    q.x[k] = x + dx[k];
                    q.y[k] = y + dy[k];
                }
            } else if (s->shape == ParticleShape::Flat) {
                const float h = p.size / 256.0f * 0.05f;
                const float cs = std::cos(p.angle) * h, sn = std::sin(p.angle) * h;
                const float cx[4] = { -cs + sn, cs + sn, -cs - sn, cs - sn }, cy[4] = { -sn - cs, sn - cs, -sn + cs, sn + cs };
                bool seen = true;
                for (int k = 0; k < 4 && seen; k++) {
                    const float corner[3] = { p.place[0] + cx[k], p.place[1] + cy[k], p.place[2] };
                    seen = project(corner, q.x[k], q.y[k], depth);
                }
                if (!seen)
                    continue;
            } else {
                const float back[3] = { p.place[0] - p.velocity[0] * p.size / 4096.0f,
                                        p.place[1] - p.velocity[1] * p.size / 4096.0f,
                                        p.place[2] - p.velocity[2] * p.size / 4096.0f };
                float x2, y2;
                if (!project(p.place, x, y, depth) || !project(back, x2, y2, depth))
                    continue;
                const float xs[4] = { x, x + 1.0f, x2, x2 + 1.0f }, ys[4] = { y, y, y2, y2 };
                for (int k = 0; k < 4; k++) {
                    q.x[k] = xs[k];
                    q.y[k] = ys[k];
                }
            }
            out.push_back(q);
        }
    };
    for (const ParticleEmitter& e : v.thingEmitters)
        draw(e);
    if (v.rainOn)
        draw(v.rain);
    return out;
}

// The flare of the flashlight (FUN_00414fa9): at the bulb (bone 2 of the
// hand, FUN_00417d64) seen in front of the camera, by how much the hand
// points at the camera (the hand's +y against the view, FUN_00417dcb): more
// than 0, the FLARE and GLOW pictures (96 pixels, their colour that much);
// more than 0.8984375, also the LINE streak, 60 pixels to each side
void placeFlare(Viewer& v, const HeldPart& hand, const BoneMatrix& wrist, const float turn[9])
{
    v.flareRects.clear();
    const HandLight& light = hand.light;
    if (!light.light || light.flare.rgba.empty() || hand.model.bones.size() < 3 || !v.sceneShown)
        return;
    useFlareHand(v, light);
    const float* rest = hand.model.bones[2].rest;
    float local[3], world[3], camera[3], aim[3];
    for (int k = 0; k < 3; k++)
        local[k] = wrist.m[0][k] * rest[0] + wrist.m[1][k] * rest[1] + wrist.m[2][k] * rest[2] + wrist.m[3][k];
    const float feet[3] = { v.mover.x, v.mover.y, v.mover.z };
    for (int k = 0; k < 3; k++)
        world[k] = turn[3 * k] * local[0] + turn[3 * k + 1] * local[1] + turn[3 * k + 2] * local[2] + feet[k];
    worldToCamera(v.scene, world, camera);
    worldDirectionToCamera(v.scene, v.beamAim, aim);
    const float facing = std::min(1.0f, -aim[1]);
    if (camera[1] <= 1.0f || facing <= 0.0f)
        return;
    float x, y;
    projectToPicture(v.scene, camera, x, y);
    // The game's colours are 0x00RRGGBB, the renderer's 0xAABBGGRR
    auto colour = [](float red, float green, float blue, float by) {
        auto part = [by](float c) { return uint32_t(std::clamp(c * by, 0.0f, 255.0f)); };
        return part(red) | part(green) << 8 | part(blue) << 16 | 0xFF000000u;
    };
    // The streak: from 0.8984375 on, (facing - 0.8984375) x 9.846 of its
    // colour (its three bytes are red, green, blue)
    const float u = 1.0f / kGlowWidth, w = 1.0f / kGlowHeight;
    const float lineTop = (kGlowLineTop + 0.5f) * w, lineBottom = (kGlowLineTop + 7.5f) * w;
    if (facing > 0.8984375f && !light.line.rgba.empty()) {
        const float by = (facing - 0.8984375f) * 9.84615421f;
        const uint32_t streak = colour(light.streakColour[0], light.streakColour[1], light.streakColour[2], by);
        v.flareRects.push_back({ x, y - 3, 60, 6, 0, lineTop, 64 * u, lineBottom, streak, true });
        v.flareRects.push_back({ x - 60, y - 3, 60, 6, 64 * u, lineTop, 0, lineBottom, streak, true });
    }
    // FLARE at half the facing, GLOW at that squared (the bytes of the hand's
    // colour are red, green, blue)
    const float half = std::min(1.0f, 0.5f * facing);
    const float red = float(light.flareColour & 255), green = float((light.flareColour >> 8) & 255),
                blue = float((light.flareColour >> 16) & 255);
    v.flareRects.push_back({ x - 48, y - 48, 96, 96, 0, 0, 64 * u, 64 * w, colour(red, green, blue, half), true });
    v.flareRects.push_back({ x - 48, y - 48, 96, 96, kGlowGlowLeft * u, 0, (kGlowGlowLeft + 64) * u, 64 * w,
                             colour(red, green, blue, half * half), true });
}

// The beam of the flashlight the character holds in its left hand, if it is
// on. As the game (FUN_00449662 places the hand's spot light, FUN_00418461,
// where the hand is and turned as it is): from the left wrist, along the
// hand's own +y (the depth of the spot's view, FUN_0041a2c0), so it follows
// the arm in every animation; the free look turns it more
void aimFlashlight(Viewer& v, const Character& c, const std::vector<BoneMatrix>& bones)
{
    v.beamOn = false;
    v.flareRects.clear();
    const int wrist = wristBone(c.model, true);
    if (actionVariation(c, v.flashlightOn) != 1 || wrist < 0 || size_t(wrist) >= bones.size())
        return;
    const BoneMatrix& b = bones[size_t(wrist)];
    float turn[9];
    turnAboutZ(v.mover.heading, turn);
    const float at[3] = { b.m[3][0], b.m[3][1], b.m[3][2] };
    const float along[3] = { b.m[1][0], b.m[1][1], b.m[1][2] };
    const float feet[3] = { v.mover.x, v.mover.y, v.mover.z };
    float aim[3];
    for (int k = 0; k < 3; k++) {
        v.beamPlace[k] = turn[3 * k] * at[0] + turn[3 * k + 1] * at[1] + turn[3 * k + 2] * at[2] + feet[k];
        aim[k] = turn[3 * k] * along[0] + turn[3 * k + 1] * along[1] + turn[3 * k + 2] * along[2];
    }
    const float length = std::sqrt(aim[0] * aim[0] + aim[1] * aim[1] + aim[2] * aim[2]);
    if (length < 1e-4f)
        return;
    // The free look: about z (to the left) and up
    const float heading = std::atan2(-aim[0], aim[1]) + v.lookYaw;
    const float pitch = std::clamp(std::asin(std::clamp(aim[2] / length, -1.0f, 1.0f)) + v.lookPitch, -1.5f, 1.5f);
    v.beamAim[0] = -std::sin(heading) * std::cos(pitch);
    v.beamAim[1] = std::cos(heading) * std::cos(pitch);
    v.beamAim[2] = std::sin(pitch);
    v.beamOn = true;
    placeFlare(v, c.leftHands[c.leftHand], b, turn);
}

// Free look (the game's, FUN_00438920 +0x98, +0x9C): standing with the
// flashlight on, the free look key and the arrows, or the left mouse button
// and the mouse, turn the flashlight; let go, it comes back. Clears the
// arrows it uses
void freeLook(Viewer& v, CharacterKeys& keys, float frameMilliseconds)
{
    const bool standing = v.mover.state == CharacterState::Idle && !keys.aim;
    const bool withKeys = keyHeld(v.gameKeys[kActionFreeLook]);
    const bool withMouse = mouseButtonHeld(false);
    const float seconds = frameMilliseconds / 1000.0f;
    float mouseX, mouseY;
    mousePixelPosition(mouseX, mouseY);
    if (v.flashlightOn && standing && (withKeys || withMouse)) {
        if (withKeys) {
            v.lookYaw += ((keys.left ? 1.0f : 0.0f) - (keys.right ? 1.0f : 0.0f)) * kLookSpeed * seconds;
            v.lookPitch += ((keys.forward ? 1.0f : 0.0f) - (keys.back ? 1.0f : 0.0f)) * kLookSpeed * seconds;
            keys.forward = keys.back = keys.left = keys.right = false;
        }
        if (withMouse) {
            if (v.mouseLooking) {
                int width, height;
                windowPixelSize(width, height);
                const float scale = 4.0f / float(std::max(width, 1));
                v.lookYaw -= (mouseX - v.mouseFrom[0]) * scale;
                v.lookPitch -= (mouseY - v.mouseFrom[1]) * scale;
            }
            v.mouseLooking = true;
        } else
            v.mouseLooking = false;
        v.lookYaw = std::clamp(v.lookYaw, -kLookSideways, kLookSideways);
        v.lookPitch = std::clamp(v.lookPitch, -kLookDown, kLookUp);
    } else {
        v.mouseLooking = false;
        const float back = std::max(0.0f, 1.0f - 6.0f * seconds);
        v.lookYaw *= back;
        v.lookPitch *= back;
    }
    v.mouseFrom[0] = mouseX;
    v.mouseFrom[1] = mouseY;
}

// Whether the level is lit (its state's bit 0x10: the lights switched on)
bool levelLit(const Viewer& v)
{
    const int level = currentLevel(v);
    return level >= 0 && level < 256 && (v.scripts.state().levels[level] & 0x10) != 0;
}

// The light of the models this frame (FUN_00426070): the level's ambient
// colour (lit or not) and the scene's light (the camera's, high up), its
// strength of 255
void showModelLight(const Viewer& v)
{
    const bool lit = levelLit(v);
    const uint8_t* colour = lit ? v.logic.ambientLit : v.logic.ambientDark;
    const float ambient[3] = { float(colour[0]) / 255.0f, float(colour[1]) / 255.0f, float(colour[2]) / 255.0f };
    const ViewScene::Light& light = v.scene.lights[lit ? 1 : 0];
    float place[3] = {};
    worldToCamera(v.scene, light.place, place);
    setModelLight(ambient, place, v.sceneShown && v.logicLoaded ? light.strength / 255.0f : 0.0f);
}

// The walk and the turn of the hero led by a scene (FUN_00438759): it has
// reached the marked place once the way to it points back from where it
// started; the turn goes evenly to its end (FUN_00438147)
void stepLedHero(Viewer& v, float frameMilliseconds)
{
    if (v.walkMarker >= 0 && size_t(v.walkMarker) < v.logic.markers.size()) {
        const LevelMarker& m = v.logic.markers[size_t(v.walkMarker)];
        const float wayX = m.place[0] - v.walkFrom[0], wayY = m.place[1] - v.walkFrom[1];
        const float leftX = m.place[0] - v.mover.x, leftY = m.place[1] - v.mover.y;
        if (wayX * leftX + wayY * leftY <= 0)
            v.walkMarker = -1;
    } else
        v.walkMarker = -1;
    if (v.turning) {
        v.turnTime += frameMilliseconds;
        if (v.turnTime >= v.turnDuration) {
            v.mover.heading = v.turnTo;
            v.turning = false;
        } else
            v.mover.heading = v.turnFrom + (v.turnTo - v.turnFrom) * v.turnTime / v.turnDuration;
    }
}

// One frame of the chosen character: led by the keys, or the animation looked
// at standing still. Shows it (or nothing)
void updateCharacter(Viewer& v, uint32_t buttons, float frameMilliseconds)
{
    if (v.character >= v.characters.size() || !v.placed || !v.sceneShown) {
        stepLevel(v, frameMilliseconds);
        updateActors(v, frameMilliseconds);
        std::vector<PosedPiece> pieces;
        if (v.sceneShown) {
            poseLevelObjects(v, pieces);
            poseLevelActors(v, pieces);
        }
        placePieces(pieces, v.scene, levelLit(v) ? 1 : 0);
        return;
    }
    const Character& character = v.characters[v.character];
    size_t animation = v.animation;
    float time = 0;
    if (!v.still) {
        const std::vector<std::array<int16_t, 10>>* actions = rightHandActions(character);
        const CharacterState state = v.mover.state;
        const int before = v.mover.animation;
        const WeaponCharacteristics* weapon = heldWeapon(character);
        v.mover.armed = weapon != nullptr;
        const CollisionMap* collision = v.logic.hasCollision ? &v.logic.collision : nullptr;
        const int asked = v.ledAnimation >= 0 ? animationWithIdentifier(character.model, v.ledAnimation) : -1;
        if (v.ledAnimation >= 0 && asked < 0)
            v.ledAnimation = -1;
        if (asked >= 0) {
            // An animation the scene asked for, to its end (FUN_00438759): over
            // once it plays itself (not the one before it waiting for a window
            // of its transition, nor the blend into it) and reaches its end;
            // a looping one plays on until another is asked for
            const AnimationClock clock = moveBody(v.mover, character.model, asked, frameMilliseconds, 0, collision);
            if (!v.mover.blending && v.mover.animation == asked && clock == AnimationClock::Ended)
                v.ledAnimation = -1;
        } else if (actions)
            moveCharacter(v.mover, character.model, *actions, actionVariation(character, v.flashlightOn),
                          v.heroLed ? v.ledButtons : buttons, frameMilliseconds, collision);
        stepLedHero(v, frameMilliseconds);
        if (weapon) {
            lockOn(v, *weapon, frameMilliseconds);
            if (v.mover.fired) {
                // The shot: +0xC (FUN_0046a0e2; +0xF is the empty click, with no bullets)
                playWeaponSound(v, 0);
                shoot(v, *weapon);
                // The clip (ours until reloading is done: full again when empty)
                auto found = v.clip.find(weapon->number);
                int& bullets = found != v.clip.end() ? found->second : (v.clip[weapon->number] = weapon->shotsPerClip);
                bullets = bullets > 1 ? bullets - 1 : std::max(weapon->shotsPerClip, 1);
            }
        }
        heroSounds(v);
        if (v.mover.state != state || v.mover.animation != before)
            showTitle(v);
        stepLevel(v, frameMilliseconds);  // before placing it: the camera may change
        animation = v.mover.animation >= 0 ? size_t(v.mover.animation) : character.model.animations.size();
        time = v.mover.time;
    } else {
        stepLevel(v, frameMilliseconds);
        if (animation < character.model.animations.size() && character.model.animations[animation].length > 0)
            // The animation, over and over (the viewer loops them all)
            time = float((milliseconds() - v.animationStart) % character.model.animations[animation].length);
    }
    updateActors(v, frameMilliseconds);
    restartAfterDeath(v);
    // The pose: the one the brain gives (it blends animations), or the
    // animation looked at standing still
    std::vector<BoneMatrix> bones;
    if (!v.still && !v.mover.pose.place.empty())
        buildSkeleton(character.model, v.mover.pose, bones);
    else
        poseSkeleton(character.model, animation, time, bones);
    // The character turns about z by its heading and stands with its feet at
    // its place
    std::vector<PosedPiece> pieces = poseCharacter(character, bones);
    for (PosedPiece& piece : pieces) {
        turnAboutZ(v.mover.heading, piece.turn);
        piece.place[0] = v.mover.x;
        piece.place[1] = v.mover.y;
        piece.place[2] = v.mover.z;
        piece.shadow = true;
        piece.shadowFloor = v.mover.z;
        piece.firstTexture = firstTextureOf(v, piece.model);
    }
    aimFlashlight(v, character, bones);
    poseLevelObjects(v, pieces);
    poseLevelActors(v, pieces);
    placePieces(pieces, v.scene, levelLit(v) ? 1 : 0);
}

// Loads the parts a character can hold: the first model of each file
void loadHeldParts(const std::string& data, const std::vector<std::string>& files, std::vector<HeldPart>& out)
{
    for (const std::string& file : files) {
        std::vector<SkeletonModel> models;
        if (loadBankModels(data, file, models) && !models.empty()) {
            HeldPart part{ file, std::move(models[0]), {} };
            loadWeaponActions(data, file, part.actions);
            loadWeaponSounds(data, file, part.soundSlots);
            loadHandLight(data, file, part.light);
            out.push_back(std::move(part));
        }
    }
}

// The animation of an action with the right hand held (variation 0), or -1
int actionAnimation(const Character& c, size_t action)
{
    const std::vector<std::array<int16_t, 10>>* actions = rightHandActions(c);
    if (!actions || action >= actions->size())
        return -1;
    return animationWithIdentifier(c.model, (*actions)[action][0]);
}

// Loads a character from the data of level 1 (levels\1\<file>): the model
// of the file with the most vertices (the character; the others are monsters
// and objects of the level)
void loadCharacter(Viewer& v, const char* name, const char* file, const char* folder)
{
    std::vector<SkeletonModel> models;
    if (!loadSkeletonModels(v.data, 1, file, models) || models.empty())
        return;
    size_t biggest = 0;
    for (size_t k = 1; k < models.size(); k++)
        if (models[k].vertices.size() > models[biggest].vertices.size())
            biggest = k;
    Character c{ name, file, std::move(models[biggest]), {}, {}, 0, 0 };
    // Left hands: lefthand\<folder>\hand0..4 (1, 3 and 4 hold the flashlight);
    // right: the empty hand (righthd, hanging, with the actions of an empty
    // hand), another one (righthand, open flat: for some gesture, not
    // studied) and the ten weapons of weapon\<folder>
    std::vector<std::string> left, right{ std::string("weapon/") + folder + "/righthd",
                                         std::string("weapon/") + folder + "/righthand" };
    for (int k = 0; k < 5; k++)
        left.push_back(std::string("lefthand/") + folder + "/hand" + std::to_string(k));
    for (int k = 0; k < 10; k++)
        right.push_back(std::string("weapon/") + folder + "/weapon" + std::to_string(k));
    loadHeldParts(v.data, left, c.leftHands);
    loadHeldParts(v.data, right, c.rightHands);
    c.leftHand = c.leftHands.size() > 1 ? 1 : 0;  // with the flashlight
    loadWeaponTable(v.data, std::string(1, folder[0]) + "weap.csv", c.weapons);  // cweap.csv, aweap.csv
    v.characters.push_back(std::move(c));
}

void changeLevel(Viewer& v, int step)
{
    v.levelIndex = (v.levelIndex + v.levels.size() + step) % v.levels.size();
    v.cameras = cameraNumbers(v.data, currentLevel(v));
    v.cameraIndex = 0;
}

// The texts of an item (read the first time)
const ItemInfo& itemInfo(Viewer& v, int item)
{
    auto found = v.items.find(item);
    if (found == v.items.end()) {
        ItemInfo info;
        if (!loadItemInfo(v.data, item, info))
            info.name = "Item " + std::to_string(item);
        found = v.items.emplace(item, info).first;
    }
    return found->second;
}

// What the scripts gave the hero with the game telling it (function 23):
// "You take..." until the use key closes it
void showPickups(Viewer& v)
{
    for (const InventoryItem& taken : v.scripts.takePickups())
        v.notice = takenText(itemInfo(v, taken.item), taken.count);
}

// Opens the inventory (the inventory key): the items carried, the last one
// taken chosen
void showMenuScreen(Viewer& v, MenuScreen screen);
void openInventory(Viewer& v)
{
    const Inventory& inventory = v.scripts.state().inventory;
    v.menu.items.clear();
    v.menu.descriptions.clear();
    v.menu.item = 0;
    for (const InventoryItem& carried : inventory.items) {
        const ItemInfo& info = itemInfo(v, carried.item);
        std::string line = info.name;
        if (carried.count > 1)
            line += "  x" + std::to_string(carried.count);
        if (carried.item == inventory.chosen)
            v.menu.item = int(v.menu.items.size());
        v.menu.items.push_back(line);
        v.menu.descriptions.push_back(info.description);
    }
    showMenuScreen(v, MenuScreen::Inventory);
}

// Uses the item chosen in the inventory: a weapon goes to the right hand
// (its model weapon\<hero>\weaponN); anything else, nothing yet. True if
// it was used (the inventory closes)
bool useInventoryItem(Viewer& v)
{
    const Inventory& inventory = v.scripts.state().inventory;
    if (size_t(v.menu.item) >= inventory.items.size() || v.character >= v.characters.size())
        return false;
    const int item = inventory.items[size_t(v.menu.item)].item;
    Character& c = v.characters[v.character];
    for (const WeaponCharacteristics& w : c.weapons) {
        if (w.item != item)
            continue;
        const std::string wanted = "/weapon" + std::to_string(w.number);
        for (size_t k = 0; k < c.rightHands.size(); k++) {
            const std::string& name = c.rightHands[k].name;
            if (name.size() >= wanted.size() && name.compare(name.size() - wanted.size(), wanted.size(), wanted) == 0) {
                c.rightHand = k;
                showViewerTextures(v);
                showTitle(v);
                return true;
            }
        }
    }
    return false;
}

// The name of the saved game (as the game's quick save, F5 and F8)
constexpr const char* kQuickSave = "quick";

// Saves the game being played (F5), and says so
void saveGame(Viewer& v)
{
    if (v.character >= v.characters.size() || !v.placed)
        return;
    SaveGame g;
    g.hero = int(v.character);
    g.level = currentLevel(v);
    g.camera = v.cameraIndex < v.cameras.size() ? v.cameras[v.cameraIndex] : -1;
    g.place[0] = v.mover.x;
    g.place[1] = v.mover.y;
    g.place[2] = v.mover.z;
    g.heading = v.mover.heading;
    g.flashlightOn = v.flashlightOn;
    g.leftHand = int(v.characters[v.character].leftHand);
    g.rightHand = int(v.characters[v.character].rightHand);
    g.state = v.scripts.state();
    g.variables = v.scripts.variables();
    const bool saved = writeSaveGame(kQuickSave, g);
    v.notice = saved ? "Game saved." : "The game could not be saved.";
    v.menu.canLoad = v.menu.canLoad || saved;
}

// Loads the saved game (F8, or the title's Load Saved Game): the hero in
// its level where it was, with what the scripts remembered; the level's
// script sets its things again. False if there is none
bool loadGame(Viewer& v)
{
    SaveGame g;
    if (!readSaveGame(kQuickSave, g) || size_t(g.hero) >= v.characters.size())
        return false;
    const auto found = std::find(v.levels.begin(), v.levels.end(), g.level);
    if (found == v.levels.end())
        return false;
    v.character = size_t(g.hero);
    Character& c = v.characters[v.character];
    if (size_t(g.leftHand) < c.leftHands.size())
        c.leftHand = size_t(g.leftHand);
    if (size_t(g.rightHand) < c.rightHands.size())
        c.rightHand = size_t(g.rightHand);
    v.levelIndex = size_t(found - v.levels.begin());
    v.fromLevel = -1;
    v.mover = CharacterMover();
    v.still = false;
    v.flashlightOn = g.flashlightOn;
    v.allActors = false;
    v.notice.clear();
    v.music.stopAll();
    v.scripts.freeMusicChannels();
    changeLevel(v, 0);
    loadLogic(v);
    v.scripts.state() = g.state;
    if (g.variables.size() == v.scripts.variables().size())
        v.scripts.variables() = g.variables;
    v.mover.x = g.place[0];
    v.mover.y = g.place[1];
    v.mover.z = g.place[2];
    v.mover.heading = g.heading;
    if (v.logic.hasCollision)
        standOnFloor(v.mover, v.logic.collision);
    v.placed = true;
    v.stepKnown = false;
    const auto camera = std::find(v.cameras.begin(), v.cameras.end(), g.camera);
    v.cameraIndex = camera != v.cameras.end() ? size_t(camera - v.cameras.begin()) : 0;
    showCamera(v);
    showViewerTextures(v);
    if (v.logicLoaded)
        v.scripts.startScript(v.logic.levelScript);
    showTitle(v);
    return true;
}

// Where each hero's new game starts (FUN_004798c9: 0x84 - 0x66 for Carnby,
// 0x84 for Aline): Carnby lands in level 30, Aline on the roof of level 132,
// both by their way in from no level
constexpr int kCarnbyStartLevel = 30, kAlineStartLevel = 132;

// A new game with a hero (0 Carnby, 1 Aline): full life, the hero's first
// level, entered as at the start of the game
void startNewGame(Viewer& v, int hero)
{
    v.character = size_t(hero) < v.characters.size() ? size_t(hero) : 0;
    const int level = hero == 1 ? kAlineStartLevel : kCarnbyStartLevel;
    const auto found = std::find(v.levels.begin(), v.levels.end(), level);
    v.levelIndex = found != v.levels.end() ? size_t(found - v.levels.begin()) : 0;
    v.fromLevel = -1;
    v.mover = CharacterMover();
    v.still = false;
    v.flashlightOn = true;
    v.allActors = false;
    changeLevel(v, 0);
    showCamera(v);
    loadLogic(v);
    v.scripts.state() = GameState();  // a new game: nothing carried, nothing happened
    v.music.stopAll();
    v.scripts.freeMusicChannels();
    v.scripts.setWeapons(&v.characters[v.character].weapons);
    v.notice.clear();
    placeCharacterAtStart(v, false);
    showTitle(v);
}

// Shows a screen of the menu (its pictures in the renderer's layer), or the
// game (the layer back to the game's font)
void showMenuScreen(Viewer& v, MenuScreen screen)
{
    v.menu.screen = screen;
    v.menuShownAt = milliseconds();
    v.useBlocked = true;
    // Only a mouse moved from now on chooses
    mouseDesktopPosition(v.menuMouse[0], v.menuMouse[1]);
    v.menuClicking = mouseButtonHeld(false);
    if (screen == MenuScreen::Playing) {
        // The texts of the game use the font of play in the menus' image
        // (without the menu files, the font of font8.pvr)
        if (v.menuLoaded)
            setOverlayImage(v.menu.atlasWidth, v.menu.atlasHeight, v.menu.atlas.data(), true);
        else if (!v.font.rgba.empty())
            setOverlayImage(v.font.width, v.font.height, v.font.rgba.data());
        setOverlay({});
    } else
        setOverlayImage(v.menu.atlasWidth, v.menu.atlasHeight, v.menu.atlas.data(), true);
}

bool menuKey(Viewer& v, int code, bool confirm, bool back);

// The mouse on the menu: moving over an entry (or a hero) chooses it, the
// left button confirms it. False to quit the program
bool menuMouse(Viewer& v)
{
    float mouseX, mouseY, x, y;
    mousePixelPosition(mouseX, mouseY);
    const bool held = mouseButtonHeld(false);
    const bool pressed = held && !v.menuClicking;
    v.menuClicking = held;
    // Moved more than a few pixels since it was last taken as still (when
    // the screen was shown, or a key was used): the window learns where the
    // mouse is when it appears, which is not a move
    float desktopX, desktopY;
    mouseDesktopPosition(desktopX, desktopY);
    const float dx = desktopX - v.menuMouse[0], dy = desktopY - v.menuMouse[1];
    const bool moved = dx * dx + dy * dy > 64.0f;
    if (moved) {
        v.menuMouse[0] = desktopX;
        v.menuMouse[1] = desktopY;
    }
    if (!pictureFromWindow(mouseX, mouseY, x, y) || (!moved && !pressed))
        return true;
    const int item = titleMenuItemAt(v.menu, x * 640.0f, y * 480.0f);
    if (item < 0)
        return true;
    if (v.menu.screen == MenuScreen::Title) {
        if (!titleEntryWorks(v.menu, item))
            return true;
        v.menu.entry = item;
    } else if (v.menu.hero != item) {
        v.menu.hero = item;
        v.menu.cardLetters = -1;
    }
    return !pressed || menuKey(v, -1, true, false);
}

void playMovies(Viewer& v, std::vector<std::pair<int, bool>> movies, Viewer::AfterMovies after);

// A sound of a menu's bank (menu\\menu2., menu\\select2.: their slots, as
// the game plays them with FUN_00471870), everywhere at full volume. Title:
// 0 its jingle (intro\\aitd4), 1 moving, 2 back, 3 confirming; choosing the
// hero: 0 back, 1 a letter typed (intro\\bip), 2 and 3 confirming
void playMenuSound(Viewer& v, const char* bank, int slot)
{
    const std::vector<int> samples = bankSamples(v, bank);
    if (slot >= 0 && size_t(slot) < samples.size())
        playSampleAt(v, samples[size_t(slot)], nullptr);
}

// A key of the menu pressed: the game's arrows choose, its use key or Enter
// confirm, Esc goes back (on the title: back to the game, or quits). False
// to quit the program
bool menuKey(Viewer& v, int code, bool confirm, bool back)
{
    const GameControls& game = v.gameKeys;
    TitleMenu& m = v.menu;
    const bool use = confirm || code == game[kActionUse];
    if (m.screen == MenuScreen::Title) {
        if (code == game[kActionUp] || code == game[kActionDown]) {
            const int step = code == game[kActionDown] ? 1 : kTitleEntries - 1;
            do
                m.entry = (m.entry + step) % kTitleEntries;
            while (!titleEntryWorks(m, m.entry));
            playMenuSound(v, "menu\\menu2.", 1);
        } else if (back) {
            if (!m.playing)
                return false;
            playMenuSound(v, "menu\\menu2.", 2);
            showMenuScreen(v, MenuScreen::Playing);
        } else if (use) {
            playMenuSound(v, "menu\\menu2.", 3);
            if (m.entry == kTitleQuit)
                return false;
            if (m.entry == kTitleLoadGame && loadGame(v)) {
                m.playing = true;
                showMenuScreen(v, MenuScreen::Playing);
            }
            if (m.entry == kTitleNewGame) {
                if (m.playing)
                    showMenuScreen(v, MenuScreen::Playing);  // resume
                else {
                    // The intro first (movie 2), then choosing the hero (FUN_00480ac6)
                    m.cardLetters = -1;
                    showMenuScreen(v, MenuScreen::ChooseHero);
                    playMovies(v, { { 2, true } }, Viewer::AfterMovies::ChooseHero);
                }
            }
        }
    } else if (m.screen == MenuScreen::Inventory) {
        const int n = int(m.items.size());
        if ((code == game[kActionUp] || code == game[kActionDown]) && n > 0)
            m.item = (m.item + (code == game[kActionDown] ? 1 : n - 1)) % n;
        else if (back || code == game[kActionInventory])
            showMenuScreen(v, MenuScreen::Playing);
        else if (use && useInventoryItem(v))
            showMenuScreen(v, MenuScreen::Playing);
    } else if (m.screen == MenuScreen::ChooseHero) {
        if ((code == game[kActionLeft] || code == game[kActionRight]) && m.hero != (code == game[kActionLeft] ? 0 : 1)) {
            m.hero = code == game[kActionLeft] ? 0 : 1;
            m.cardLetters = -1;  // its card comes in again
        } else if (back) {
            playMenuSound(v, "menu\\select2.", 0);
            showMenuScreen(v, MenuScreen::Title);
        } else if (use) {
            playMenuSound(v, "menu\\select2.", 2);
            playMenuSound(v, "menu\\select2.", 3);
            showMenuScreen(v, MenuScreen::Loading);  // "LOADING...", then the game
        }
    }
    return true;
}

// A frame while a script shows a full-screen picture (functions 8, 63): the
// game stops; the use key, Enter, Esc or a click close it, or its time runs
// out. Then the camera comes back. False if there is none (a game frame)
bool documentFrame(Viewer& v, const std::vector<int>& codes, const std::vector<KeyPress>& keys)
{
    const LevelScripts::Document* document = v.scripts.document();
    if (!document) {
        if (!v.documentShown.empty()) {
            v.documentShown.clear();
            showCamera(v);  // the picture of the camera, its mesh and light back
        }
        return false;
    }
    if (v.documentShown != document->name) {
        DocumentPicture picture;
        if (!loadDocumentPicture(v.data, document->name, picture)) {
            v.scripts.closeDocument();  // missing: as if seen
            return false;
        }
        setPictures(picture.width, picture.height, picture.rgba.data(), picture.rgba.data());
        setForegroundMesh({}, {}, 0, 0, 0, nullptr);
        setModel({}, {});
        setLight(1.0f);
        setFlashlight(false, std::array<float, 3>{}.data(), std::array<float, 3>{}.data(), kFlashlightReach);
        setOverlay({});
        v.documentShown = document->name;
        v.documentSince = milliseconds();
        v.useBlocked = true;
    }
    bool close = document->milliseconds >= 0 && milliseconds() - v.documentSince >= uint64_t(document->milliseconds);
    for (int code : codes)
        close = close || code == v.gameKeys[kActionUse];
    for (KeyPress key : keys)
        close = close || key == KeyPress::Confirm || key == KeyPress::Quit;
    const bool clicking = mouseButtonHeld(false);
    close = close || (clicking && !v.menuClicking);
    v.menuClicking = clicking;
    // At least a moment on the screen, so that the key that opened it does
    // not close it
    if (close && milliseconds() - v.documentSince > 300)
        v.scripts.closeDocument();
    return true;
}

// Starts the next video of the queue (one that cannot be read is passed)
bool startNextMovie(Viewer& v)
{
    while (!v.movieQueue.empty()) {
        const auto found = v.movies.find(v.movieQueue.front().first);
        if (found != v.movies.end()
            && openVideo((std::filesystem::path(v.data) / "movies" / (found->second + ".bik")).string())) {
            v.moviePlaying = true;
            v.movieSince = milliseconds();
            v.movieFrame.assign(size_t(640) * 480, 0xFF000000u);
            setPictures(640, 480, v.movieFrame.data(), v.movieFrame.data());
            setForegroundMesh({}, {}, 0, 0, 0, nullptr);
            setModel({}, {});
            setLight(1.0f);
            setFlashlight(false, std::array<float, 3>{}.data(), std::array<float, 3>{}.data(), kFlashlightReach);
            setOverlay({});
            setGlows({});
            v.music.stopAll();
            stopAllVoices();
            return true;
        }
        v.movieQueue.erase(v.movieQueue.begin());
    }
    return false;
}

// Plays videos in turn, then what comes after them
void playMovies(Viewer& v, std::vector<std::pair<int, bool>> movies, Viewer::AfterMovies after)
{
    v.movieQueue = std::move(movies);
    v.afterMovies = after;
    v.moviePlaying = false;
}

// A frame while a video plays (FUN_004811cf): its picture in the middle of
// the screen (640 x 320 between black bands); the use key, Enter or a click
// skip it if it may be skipped, Esc any. When the queue is over, what comes
// after. False if no video plays (a frame of the menu or the game)
bool movieFrame(Viewer& v, const std::vector<int>& codes, const std::vector<KeyPress>& keys)
{
    if (v.movieQueue.empty()) {
        if (v.scripts.movie() >= 0) {  // a script asks for one (function 42)
            v.movieQueue.push_back({ v.scripts.movie(), true });
            v.afterMovies = Viewer::AfterMovies::Script;
        } else
            return false;
    }
    if (!v.moviePlaying && !startNextMovie(v)) {
        if (v.afterMovies == Viewer::AfterMovies::Script)
            v.scripts.closeMovie();
        return false;
    }
    int width = 0, height = 0;
    std::vector<uint32_t> picture;
    if (videoPicture(double(milliseconds() - v.movieSince), width, height, picture) && width <= 640 && height <= 480) {
        const int top = (480 - height) / 2, left = (640 - width) / 2;
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++)
                v.movieFrame[size_t(top + y) * 640 + size_t(left + x)] = picture[size_t(y) * size_t(width) + size_t(x)] | 0xFF000000u;
        setPictures(640, 480, v.movieFrame.data(), v.movieFrame.data());
    }
    bool skip = false;
    if (v.movieQueue.front().second && milliseconds() - v.movieSince > 300) {
        for (int code : codes)
            skip = skip || code == v.gameKeys[kActionUse];
        for (KeyPress key : keys)
            skip = skip || key == KeyPress::Confirm;
        const bool clicking = mouseButtonHeld(false);
        skip = skip || (clicking && !v.menuClicking);
        v.menuClicking = clicking;
    }
    // Esc skips any of them, also those the game lets play to their end (an
    // improvement of ours)
    for (KeyPress key : keys)
        skip = skip || key == KeyPress::Quit;
    if (!skip && !videoFinished())
        return true;
    closeVideo();
    v.moviePlaying = false;
    v.movieQueue.erase(v.movieQueue.begin());
    if (!v.movieQueue.empty())
        return true;
    // The queue is over
    switch (v.afterMovies) {
    case Viewer::AfterMovies::Title:
        showMenuScreen(v, MenuScreen::Title);
        playMenuSound(v, "menu\\menu2.", 0);
        break;
    case Viewer::AfterMovies::ChooseHero:
        v.menu.cardLetters = -1;
        showMenuScreen(v, MenuScreen::ChooseHero);
        break;
    case Viewer::AfterMovies::Script:
        v.scripts.closeMovie();
        showCamera(v);
        showMenuScreen(v, MenuScreen::Playing);
        break;
    case Viewer::AfterMovies::NewGame:
        showMenuScreen(v, MenuScreen::Playing);
        startNewGame(v, v.menu.hero);
        break;
    default:
        break;
    }
    v.afterMovies = Viewer::AfterMovies::Nothing;
    v.useBlocked = true;
    return true;
}

bool hasGame(const std::string& folder)
{
    std::error_code error;
    return std::filesystem::is_directory(std::filesystem::path(folder) / "levels", error);
}

} // namespace

int main(int argc, char* argv[])
{
    Viewer viewer;
    RendererChoice renderer = RendererChoice::Auto;
    int startLevel = -1, startCamera = -1;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--data") == 0 && i + 1 < argc)
            viewer.data = argv[++i];
        else if (std::strcmp(argv[i], "--renderer") == 0 && i + 1 < argc)
            renderer = parseRendererChoice(argv[++i]);
        else if (std::strcmp(argv[i], "--level") == 0 && i + 1 < argc)
            startLevel = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--camera") == 0 && i + 1 < argc)
            startCamera = std::atoi(argv[++i]);
    }
    if (viewer.data.empty())
        viewer.data = std::filesystem::current_path().string();

    if (!openWindow(kTitle, 1280, 960)) {
        std::fprintf(stderr, "SDL: %s\n", lastWindowError().c_str());
        return 1;
    }
    if (!hasGame(viewer.data) || (viewer.levels = levelNumbers(viewer.data)).empty()) {
        showErrorMessage(kTitle, "The game was not found in:\n" + viewer.data
                                     + "\n\nStart it with --data \"<folder of Alone in the Dark: The New Nightmare>\".");
        closeWindow();
        return 1;
    }

    int width, height;
    windowPixelSize(width, height);
    if (!initRenderer(renderer, width, height)) {
        showErrorMessage(kTitle, "The renderer could not be started.");
        closeWindow();
        return 1;
    }
    // Start at the level and camera asked for, if they exist
    const auto level = std::find(viewer.levels.begin(), viewer.levels.end(), startLevel);
    if (level != viewer.levels.end())
        viewer.levelIndex = size_t(level - viewer.levels.begin());
    loadGameControls(viewer.data, viewer.gameKeys);
    loadNpcTable(viewer.data, viewer.npcs);
    // Sound always through Windows' default output, at full volume
    if (startAudio())
        loadSoundBanks(viewer.data, viewer.soundBanks);
    viewer.music.setup(&viewer.soundBanks, [&viewer](const std::string& name) { return sampleNamed(viewer, name); });
    if (loadGameFont(viewer.data, viewer.font))
        setOverlayImage(viewer.font.width, viewer.font.height, viewer.font.rgba.data());
    // Without a level asked for, the game starts at its title
    viewer.menuLoaded = loadTitleMenu(viewer.data, viewer.menu);
    loadBulletPictures(viewer);
    {
        SaveGame saved;
        viewer.menu.canLoad = readSaveGame(kQuickSave, saved);
    }
    loadCharacter(viewer, "Carnby", "Carnby01", "carnby");
    loadCharacter(viewer, "Aline", "Aline01", "aline");
    showViewerTextures(viewer);
    changeLevel(viewer, 0);
    const auto camera = std::find(viewer.cameras.begin(), viewer.cameras.end(), startCamera);
    if (camera != viewer.cameras.end())
        viewer.cameraIndex = size_t(camera - viewer.cameras.begin());
    showCamera(viewer);
    loadLogic(viewer);
    placeCharacterAtStart(viewer, camera != viewer.cameras.end());  // a camera asked for stays

    viewer.movies = loadMovieList(viewer.data);
    viewer.voices.load(viewer.data);
    if (viewer.menuLoaded && startLevel < 0) {
        // As the game starts (FUN_004798c9): the logos and Spiral House's
        // (the first time not skipped), the trailer, then the title
        showMenuScreen(viewer, MenuScreen::Title);
        playMovies(viewer, { { 0, false }, { 56, true }, { 3, true } }, Viewer::AfterMovies::Title);
    }
    else if (viewer.menuLoaded)
        showMenuScreen(viewer, MenuScreen::Playing);
    else
        viewer.menu.screen = MenuScreen::Playing;
    std::vector<KeyPress> keys;
    std::vector<int> codes;  // keys of the game pressed (DirectInput codes)
    uint64_t lastFrame = milliseconds();
    while (pollWindowEvents(keys, codes)) {
        // A video: only its keys
        if (movieFrame(viewer, codes, keys)) {
            codes.clear();
            keys.clear();
            windowPixelSize(width, height);
            lastFrame = milliseconds();
            drawFrame(width, height);
            continue;
        }
        // The menu: only its keys, and nothing of the game moves
        if (viewer.menu.screen != MenuScreen::Playing) {
            setGlows({});
            bool quit = false;
            if (!codes.empty() || !keys.empty())  // the keys take over from the mouse
                mouseDesktopPosition(viewer.menuMouse[0], viewer.menuMouse[1]);
            for (int code : codes)
                quit = quit || !menuKey(viewer, code, false, false);
            for (KeyPress key : keys)
                if (key == KeyPress::Confirm || key == KeyPress::Quit)
                    quit = quit || !menuKey(viewer, -1, key == KeyPress::Confirm, key == KeyPress::Quit);
            codes.clear();
            keys.clear();
            quit = quit || !menuMouse(viewer);
            if (quit)
                break;
            if (viewer.menu.screen == MenuScreen::ChooseHero) {
                const uint64_t now = milliseconds();
                const int typed = stepHeroCard(viewer.menu, float(std::min<uint64_t>(now - lastFrame, 100)));
                lastFrame = now;
                if (typed > 0)
                    playMenuSound(viewer, "menu\\select2.", 1);  // a click (the game: one a letter)
            }
            // "LOADING..." a moment, so that the change is seen, then the game
            if (viewer.menu.screen == MenuScreen::Loading && milliseconds() - viewer.menuShownAt > 1200) {
                viewer.menu.playing = true;
                // The hero's video first, not skipped (FUN_004798c9: select_c 5
                // for Carnby, select_a 4 for Aline), then the game starts
                playMovies(viewer, { { viewer.menu.hero == 0 ? 5 : 4, false } }, Viewer::AfterMovies::NewGame);
            }
            if (viewer.menu.screen != MenuScreen::Playing)
                setOverlay(titleMenuRects(viewer.menu, float(milliseconds() - viewer.menuShownAt) / 1000.0f));
            windowPixelSize(width, height);
            viewer.music.update(float(std::min<uint64_t>(milliseconds() - lastFrame, 100)));
            lastFrame = milliseconds();
            drawFrame(width, height);
            continue;
        }
        // A picture of a script on the whole screen: the game waits
        if (documentFrame(viewer, codes, keys)) {
            codes.clear();
            keys.clear();
            windowPixelSize(width, height);
            viewer.music.update(float(std::min<uint64_t>(milliseconds() - lastFrame, 100)));
            lastFrame = milliseconds();
            drawFrame(width, height);
            continue;
        }
        bool quit = false;
        // The game's keys: use (action, or answer and close what a script
        // shows), left and right (choose an answer), flashlight
        for (int code : codes) {
            const GameControls& game = viewer.gameKeys;
            const LevelScripts::Question* question = viewer.scripts.question();
            if (code == game[kActionUse]) {
                if (question)
                    viewer.scripts.answer(viewer.chosenAnswer);
                else if (viewer.scripts.message())
                    viewer.scripts.closeMessage();
                else if (!viewer.notice.empty())
                    viewer.notice.clear();
                else if (!aimingState(viewer.mover.state) && !keyHeld(game[kActionAim]))
                    doAction(viewer);  // aiming, it shoots instead
            } else if (question && (code == game[kActionLeft] || code == game[kActionRight])) {
                const int n = int(question->answers.size());
                if (n > 0)
                    viewer.chosenAnswer = (viewer.chosenAnswer + (code == game[kActionRight] ? 1 : n - 1)) % n;
            } else if (code == game[kActionInventory] && viewer.menuLoaded && !question) {
                openInventory(viewer);
            } else if (code == game[kActionFlashlight]) {
                viewer.flashlightOn = !viewer.flashlightOn;
                showCamera(viewer);
            }
        }
        codes.clear();
        for (KeyPress key : keys) {
            switch (key) {
            case KeyPress::PreviousCamera:
            case KeyPress::NextCamera:
                if (!viewer.cameras.empty()) {
                    const size_t n = viewer.cameras.size();
                    viewer.cameraIndex = (viewer.cameraIndex + n + (key == KeyPress::NextCamera ? 1 : n - 1)) % n;
                }
                showCamera(viewer);  // the character stays where it is in the world
                break;
            case KeyPress::PreviousLevel:
            case KeyPress::NextLevel:
                viewer.fromLevel = -1;  // jumped, not walked: as starting the game
                changeLevel(viewer, key == KeyPress::NextLevel ? 1 : -1);
                showCamera(viewer);
                loadLogic(viewer);
                placeCharacterAtStart(viewer, false);
                break;
            case KeyPress::PlaceCharacter: {
                float mouseX, mouseY, x, y;
                mousePixelPosition(mouseX, mouseY);
                const float heading = viewer.mover.heading;
                if (pictureFromWindow(mouseX, mouseY, x, y) && placeCharacter(viewer, x * kViewWidth, y * kViewHeight))
                    viewer.mover.heading = heading;  // only moved, not turned
                break;
            }
            case KeyPress::Confirm:
                if (viewer.scripts.question())
                    viewer.scripts.answer(viewer.chosenAnswer);
                else if (viewer.scripts.message())
                    viewer.scripts.closeMessage();
                break;
            case KeyPress::NextLight:
                viewer.light = viewer.light == LightMode::Flashlight ? LightMode::Lit
                               : viewer.light == LightMode::Lit      ? LightMode::Dark
                                                                     : LightMode::Flashlight;
                showCamera(viewer);
                break;
            case KeyPress::NextCharacter:
                viewer.character = viewer.character < viewer.characters.size() ? viewer.character + 1 : 0;
                viewer.animation = 0;
                viewer.animationStart = milliseconds();
                viewer.mover.animation = -1;  // the other model's animations
                viewer.mover.blending = false;
                viewer.mover.pose = {};
                loadLogic(viewer);            // each one has its own collision map
                if (!viewer.placed)
                    placeCharacterAtStart(viewer, true);
                showViewerTextures(viewer);
                showTitle(viewer);
                break;
            case KeyPress::NextLeftHand:
            case KeyPress::NextRightHand:
                if (viewer.character < viewer.characters.size()) {
                    Character& c = viewer.characters[viewer.character];
                    const bool left = key == KeyPress::NextLeftHand;
                    const size_t n = (left ? c.leftHands : c.rightHands).size();
                    size_t& chosen = left ? c.leftHand : c.rightHand;
                    chosen = chosen < n ? chosen + 1 : 0;  // n: none
                    showViewerTextures(viewer);
                    showTitle(viewer);
                }
                break;
            case KeyPress::NextAction:
                if (viewer.character < viewer.characters.size()) {
                    // The next action that has an animation with this weapon
                    const Character& c = viewer.characters[viewer.character];
                    const size_t count = sizeof(kActionNames) / sizeof(kActionNames[0]);
                    for (size_t step = 1; step <= count; step++) {
                        const size_t action = (viewer.byAction ? viewer.action + step : step - 1) % count;
                        const int animation = actionAnimation(c, action);
                        if (animation >= 0) {
                            viewer.action = action;
                            viewer.animation = size_t(animation);
                            viewer.byAction = true;
                            viewer.still = true;
                            viewer.animationStart = milliseconds();
                            break;
                        }
                    }
                    showTitle(viewer);
                }
                break;
            case KeyPress::NextAnimation:
            case KeyPress::PreviousAnimation:
                if (viewer.character < viewer.characters.size()) {
                    const size_t n = viewer.characters[viewer.character].model.animations.size();
                    if (n > 0)
                        viewer.animation = (viewer.animation + (key == KeyPress::NextAnimation ? 1 : n - 1)) % n;
                    viewer.byAction = false;
                    viewer.still = true;
                    viewer.animationStart = milliseconds();
                    showTitle(viewer);
                }
                break;
            case KeyPress::ToggleForeground:
                viewer.highlight = !viewer.highlight;
                setForegroundHighlight(viewer.highlight);
                break;
            case KeyPress::NextSound:
                // The next slot of the level's bank with a sample, at full volume
                for (size_t k = 0; k < viewer.levelSounds.size(); k++) {
                    viewer.soundTried = (viewer.soundTried + 1) % viewer.levelSounds.size();
                    if (viewer.levelSounds[viewer.soundTried] >= 0) {
                        playSound(viewer.levelSounds[viewer.soundTried], 1.0f, 0.5f, 1.0f);
                        break;
                    }
                }
                break;
            case KeyPress::ToggleActors:
                // Every actor away appears, as a script would make it (order
                // 1); again, every one here goes away (order 3)
                viewer.allActors = !viewer.allActors;
                for (LevelActor& actor : viewer.logic.actors)
                    if (viewer.allActors && actor.presence == ActorPresence::Away) {
                        actor.presence = ActorPresence::Here;
                        actor.shown = true;
                    } else if (!viewer.allActors && actor.presence == ActorPresence::Here) {
                        actor.presence = ActorPresence::Away;
                        actor.shown = false;
                    }
                break;
            case KeyPress::NextRenderer:
                // Stop bgfx and start it again with the next renderer
                shutdownRenderer();
                renderer = nextRendererChoice(renderer);
                windowPixelSize(width, height);
                if (!initRenderer(renderer, width, height)) {
                    showErrorMessage(kTitle, "That renderer could not be started; back to Direct3D 11.");
                    shutdownRenderer();
                    renderer = RendererChoice::Direct3D11;
                    initRenderer(renderer, width, height);
                }
                showCamera(viewer);
                break;
            case KeyPress::QuickSave:
                saveGame(viewer);
                break;
            case KeyPress::QuickLoad:
                if (!loadGame(viewer))
                    viewer.notice = "There is no saved game.";
                break;
            case KeyPress::Quit:
                // Esc: the title, as the game's pause (without the game's
                // menu files, it quits)
                if (viewer.menuLoaded) {
                    viewer.menu.playing = true;
                    viewer.menu.entry = kTitleNewGame;
                    showMenuScreen(viewer, MenuScreen::Title);
                } else
                    quit = true;
                break;
            }
        }
        keys.clear();
        if (quit)
            break;
        windowPixelSize(width, height);

        // The character: the arrows lead it again after looking at an animation
        const uint64_t now = milliseconds();
        const float frameMilliseconds = float(std::min<uint64_t>(now - lastFrame, 100));  // not a jump after a pause
        lastFrame = now;
        const GameControls& game = viewer.gameKeys;
        CharacterKeys keysHeld;
        keysHeld.forward = keyHeld(game[kActionUp]);
        keysHeld.back = keyHeld(game[kActionDown]);
        keysHeld.left = keyHeld(game[kActionLeft]);
        keysHeld.right = keyHeld(game[kActionRight]);
        keysHeld.run = keyHeld(game[kActionRun]);
        keysHeld.aim = keyHeld(game[kActionAim]);
        if (viewer.useBlocked && !keyHeld(game[kActionUse]) && !keyHeld(0x1C))
            viewer.useBlocked = false;
        keysHeld.action = keyHeld(game[kActionUse]) && !viewer.useBlocked;
        freeLook(viewer, keysHeld, frameMilliseconds);
        // While a script asks or says something, the character waits (the
        // arrows choose)
        const bool asking =
            viewer.scripts.question() != nullptr || viewer.scripts.message() != nullptr || !viewer.notice.empty();
        if (!asking)
            viewer.chosenAnswer = 0;
        const uint32_t buttons = asking ? 0 : characterButtons(viewer.controls, keysHeld, frameMilliseconds);
        if (viewer.still && (keysHeld.forward || keysHeld.back || keysHeld.left || keysHeld.right)) {
            viewer.still = false;
            showTitle(viewer);
        }
        updateCharacter(viewer, buttons, frameMilliseconds);
        showLight(viewer);
        showModelLight(viewer);
        playScriptSounds(viewer);
        viewer.music.update(frameMilliseconds);
        showPickups(viewer);
        showQuestion(viewer);
        drawFrame(width, height);
    }

    stopAudio();
    shutdownRenderer();
    closeWindow();
    return 0;
}
