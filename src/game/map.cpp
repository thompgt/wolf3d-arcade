#include "map.h"

#include <cassert>

namespace wolf {
namespace {

// ASCII legend
// ------------
//   .  floor                 #  grey brick          B  blue stone
//   @  player start          W  wood panel          M  mossy stone
//   =  door                  S  steel
//   $  gold-locked door      %  silver-locked door
//   P  pushwall (secret)     E  exit switch
//   g  guard                 z  SS trooper
//   h  health                a  ammo                t  treasure
//   k  gold key              l  silver key
//   m  machine gun           c  chaingun
//   o  lamp (blocks)         x  table (blocks)
//
// Layout, north at the top:
//
//   rows  1- 9  cell block (start) | guard room | storeroom (silver-locked)
//   row  10     divider with three doors
//   rows 11-12  east-west spine corridor
//   row  13     divider with two doors
//   rows 14-27  mess hall | courtyard (pillared)
//   row  28     divider; the courtyard door south is gold-locked
//   rows 29-46  armory (weapons) | exit hall, with a pushwall vault SE
//   row  47     south border, exit switch at col 30
constexpr int kLevelSize = 48;

const std::vector<std::string>& level1Rows() {
    static const std::vector<std::string> rows = {
        "################################################", //  0
        "#....#....#....#...............#...............#", //  1
        "#....#....#....#.......g.......#..a.........t..#", //  2
        "#.h..#....#..a.#....o......o...#........g......#", //  3
        "#..............#............g..#...............#", //  4
        "#..............#...............#...............#", //  5
        "#.....x........#...BBB...BBB...#...............#", //  6
        "#..............#...............#.......k.......#", //  7
        "#..@...........#..g............#...............#", //  8
        "#..............#...............#...............#", //  9
        "#######=###############=###############%########", // 10
        "#..................................g...........#", // 11
        "#...................a..........................#", // 12
        "#######=######################=#################", // 13
        "#...............#..............................#", // 14
        "#..x.x..........#........g.....................#", // 15
        "#...............#..............................#", // 16
        "#.......h.......#.....MM......MM......MM.......#", // 17
        "#...............#.......................g......#", // 18
        "#...............#..............................#", // 19
        "#...........a...=..............................#", // 20
        "#...............#.....MM......MM......MM.......#", // 21
        "#...............#................z.............#", // 22
        "#......l........#..............................#", // 23
        "#...............#..............................#", // 24
        "#...............#...........................t..#", // 25
        "#...............#..............................#", // 26
        "#...............#..............................#", // 27
        "#######=######################$#################", // 28
        "#...............#..............................#", // 29
        "#...m...........#...g..........................#", // 30
        "#...............#..............................#", // 31
        "#...........c...#.....BB......BB......BB.......#", // 32
        "#...............#..............................#", // 33
        "#......g........#..............z...............#", // 34
        "#...............=..............................#", // 35
        "#...............#..............................#", // 36
        "#...............#.....BB......BB......BB.......#", // 37
        "#...............#..........z...................#", // 38
        "#...............#..............................#", // 39
        "#...............#.......................SSSSSSSS", // 40
        "#......h........#.......................S......S", // 41
        "#...............#.......................St.t.t.S", // 42
        "#...g...........#.......................P......S", // 43
        "#...............#.......................S.tttt.S", // 44
        "#......a........#.......................S......S", // 45
        "#...............#.......................SSSSSSSS", // 46
        "##############################E#################", // 47
    };
    return rows;
}

// Maps a wall glyph to its texture, or returns false if the glyph is not a
// wall at all.
bool wallTexture(char c, uint8_t& tex) {
    switch (c) {
    case '#': tex = TexBrick;     return true;
    case 'B': tex = TexBlueStone; return true;
    case 'W': tex = TexWood;      return true;
    case 'M': tex = TexMoss;      return true;
    case 'S': tex = TexSteel;     return true;
    default:  return false;
    }
}

// Maps a spawn glyph to its kind, or returns false for anything else.
bool spawnKind(char c, SpawnKind& kind) {
    switch (c) {
    case '@': kind = SpawnKind::PlayerStart; return true;
    case 'g': kind = SpawnKind::Guard;       return true;
    case 'z': kind = SpawnKind::SS;          return true;
    case 'h': kind = SpawnKind::Health;      return true;
    case 'a': kind = SpawnKind::Ammo;        return true;
    case 't': kind = SpawnKind::Treasure;    return true;
    case 'k': kind = SpawnKind::GoldKey;     return true;
    case 'l': kind = SpawnKind::SilverKey;   return true;
    case 'm': kind = SpawnKind::MachineGun;  return true;
    case 'c': kind = SpawnKind::Chaingun;    return true;
    case 'o': kind = SpawnKind::Lamp;        return true;
    case 'x': kind = SpawnKind::Table;       return true;
    default:  return false;
    }
}

} // namespace

Map Map::level1() {
    Map m;
    m.parse(level1Rows());
    return m;
}

void Map::parse(const std::vector<std::string>& rows) {
    h_ = static_cast<int>(rows.size());
    assert(h_ > 0);
    w_ = static_cast<int>(rows[0].size());
    assert(w_ == kLevelSize && h_ == kLevelSize);

    cells_.assign(static_cast<size_t>(w_) * h_, MapCell{});

    for (int y = 0; y < h_; ++y) {
        // A short or long row would silently shift the entire level, so it
        // is worth catching loudly at load rather than debugging visually.
        assert(static_cast<int>(rows[y].size()) == w_);
        for (int x = 0; x < w_; ++x) {
            const char c = rows[y][x];
            MapCell& cell = cells_[static_cast<size_t>(y) * w_ + x];

            uint8_t tex = TexBrick;
            SpawnKind kind{};

            if (wallTexture(c, tex)) {
                cell.kind = TileKind::Wall;
                cell.tex  = tex;
            } else if (c == '=') {
                cell.kind = TileKind::Door;
                cell.tex  = TexDoor;
            } else if (c == '$') {
                cell.kind = TileKind::DoorGold;
                cell.tex  = TexDoor;
            } else if (c == '%') {
                cell.kind = TileKind::DoorSilver;
                cell.tex  = TexDoor;
            } else if (c == 'P') {
                // A pushwall must look like its neighbours or it isn't a
                // secret; it borrows the steel vault texture here.
                cell.kind = TileKind::Pushwall;
                cell.tex  = TexSteel;
            } else if (c == 'E') {
                cell.kind = TileKind::ExitSwitch;
                cell.tex  = TexExitSwitch;
            } else if (spawnKind(c, kind)) {
                cell.kind = TileKind::Empty;
                spawns_.push_back(Spawn{kind, x, y});
                if (kind == SpawnKind::PlayerStart) {
                    start_x_ = x + 0.5;
                    start_y_ = y + 0.5;
                }
            } else {
                assert(c == '.' && "unknown map glyph");
                cell.kind = TileKind::Empty;
            }
        }
    }
}

const MapCell& Map::at(int x, int y) const {
    if (x < 0 || x >= w_ || y < 0 || y >= h_) return solid_;
    return cells_[static_cast<size_t>(y) * w_ + x];
}

bool Map::isSolid(int x, int y) const {
    return at(x, y).kind != TileKind::Empty;
}

bool Map::isDoor(int x, int y) const {
    const TileKind k = at(x, y).kind;
    return k == TileKind::Door || k == TileKind::DoorGold ||
           k == TileKind::DoorSilver;
}

} // namespace wolf
