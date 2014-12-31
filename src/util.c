#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>


/*

MSG_NO_ELBOW_ROOM:  "You don't have enough elbow-room to maneuver.";

MSG_NOTHING_HAPPENS
struct c_common_strings c_common_strings = {
        "Nothing happens.",             "That's enough tries!",
        "That is a silly thing to %s.", "shudder for a moment.",
        "something", "Something", "You can move again.", "Never mind.",
        "vision quickly clears.", {"the", "your"}
};


MSG_FOUND_SECRET_DOOR, "You hear a hollow sound.  This must be a secret %s!";
MSG_FOUND_SECRET_PASSAGE, "You hear a hollow sound.  This must be a secret %s!";

MSG_WHISTLE_SHRILL, "You produce a %s whistling sound.";
MSG_WHISTLE_HIGH,"You produce a %s whistling sound.";

MSG_WHISTLE_MAGIC
        You("produce a %s whistling sound", Hallucination() ? "normal" : "strange");

MSG_FAILED_POLYMORPH, "%(monster_subject) shudders!"

MSG_YOU_LOOK_COLOR,
static const char look_str[] = "look %s.";
                You(look_str, hcolor(NULL));
static const char * const hcolors[] = {
MSG_YOU_LOOK_PEAKED
                You(look_str, "peaked");
MSG_YOU_LOOK_UNDERNOURISHED,
                You(look_str, "undernourished");

message_const(MSG_YOU_HAVE_NO_REFLECTION);
    You("don't have a reflection.");

message_const(MSG_HUH_NO_LOOK_LIKE_YOU);
    pline("Huh?  That doesn't look like you!");

message_const(MSG_YOU_STIFFEN_MOMENTARILY_UNDER_YOUR_GAZE);
    You("stiffen momentarily under your gaze.");

message_const(MSG_MIRROR_STARES_BACK);
    pline(Hallucination() ?
        "Yow!  The mirror stares back!" :
        "Yikes!  You've frozen yourself!");

strcpy(buf, xname(uarmc));
You(need_to_remove_outer_armor, buf, xname(otmp));
message_object2(MSG_YOU_MUST_REMOVE_O_TO_GREASE_O, uarmc, otmp);
static const char need_to_remove_outer_armor[] = "need to remove your %s to grease your %s.";

strcpy(buf, uarmc ? xname(uarmc) : "");
if (uarmc && uarm) strcat(buf, " and ");
strcat(buf, uarm ? xname(uarm) : "");
You(need_to_remove_outer_armor, buf, xname(otmp));
message_object3(MSG_YOU_MUST_REMOVE_O_AND_O_TO_GREASE_O, uarmc, uarm, otmp);
static const char need_to_remove_outer_armor[] = "need to remove your %s to grease your %s.";

MSG_NOT_ENOUGH_ROOM_TO_USE = "There's not enough room here to use that.",

message_const(MSG_NO_HIT_IF_CANNOT_SEE_SPOT);
    cant_see_spot[] = "won't hit anything if you can't see that spot.",

message_const(MSG_YOU_CANNOT_REACH_SPOT_FROM_HERE);
    cant_reach[] = "can't reach that spot from here.";

message_const(MSG_USING_CAMERA_UNDERWATER);
    pline("Using your camera underwater would void the warranty.");

MSG_YOU_TAKE_PICTURE_OF_SWALLOW
        char name[BUFSZ];
        mon_nam(name, BUFSZ, u.ustuck);
        You("take a picture of %s%s %s.", name, possessive_suffix(name), mbodypart(u.ustuck, STOMACH));

MSG_YOU_TAKE_PICTURE_OF_DUNGEON
        You("take a picture of the %s.", (u.dz > 0) ? surface(u.ux,u.uy) : ceiling(u.ux,u.uy));

message_const(MSG_YOU_HAVE_NO_FREE_HAND);
You("have no free %s!", body_part(HAND));

message_const(MSG_CANNOT_USE_WHILE_WEARING);
You("cannot use it while you're wearing it!");

message_const(old ? MSG_YOUR_HANDS_FILTHIER : MSG_YOUR_HANDS_GET_SLIMY)
Your("%s %s!", makeplural(body_part(HAND)),
        (old ? "are filthier than ever" : "get slimy"));

message_const(old ? MSG_YOUR_FACE_HAS_MORE_GUNK : MSG_YOUR_FACE_NOW_HAS_GUNK);
pline("Yecch! Your %s %s gunk on it!", body_part(FACE),
    (old ? "has more" : "now has"));

const char *what = (ublindf->otyp == LENSES) ?  "lenses" : "blindfold";
if (ublindf->cursed) {
    message_const(MSG_YOU_PUSH_YOUR_LENSES_CROOKED);
    You("push your %s %s.", what, rn2(2) ? "cock-eyed" : "crooked");
} else {
    struct obj *saved_ublindf = ublindf;
    message_const(MSG_YOU_PUSH_YOUR_LENSES_OFF);
    You("push your %s off.", what);
    Blindf_off(ublindf);
    dropx(saved_ublindf);
}

message_const(MSG_YOU_WIPE_OFF_YOUR_HANDS);
You("wipe off your %s.", makeplural(body_part(HAND)));

message_const(MSG_YOU_GOT_THE_GLOP_OFF);
pline("You've got the glop off.");

message_const(MSG_YOUR_FACE_FEELS_CLEAN_NOW);
Your("%s feels clean now.", body_part(FACE));

    message_const(MSG_YOUR_FACE_AND_HAND_ARE_CLEAN);
    Your("%s and %s are already clean.",
            body_part(FACE), makeplural(body_part(HAND)));

        message_const(MSG_ITS_DEAD_JIM);
        You_hear("a voice say, \"It's dead, Jim.\"");

            message_const(MSG_YOU_DETERMINE_ITS_DEAD);
            You("determine that %s unfortunate being is dead.",
                    (rx == u.ux && ry == u.uy) ? "this" : "that");

            message_object((ttmp && ttmp->ttyp == STATUE_TRAP) ?
                    MSG_STATUE_APPEARS_EXCELLENT : MSG_STATUE_APPEARS_EXTRAORDINARY);
            pline("%s appears to be in %s health for a statue.",
                    The(mons[otmp->corpsenm].mname),
                    (ttmp && ttmp->ttyp == STATUE_TRAP) ?  "extraordinary" : "excellent");

        message_const(MSG_YOU_HAVE_NO_HANDS);
        You("have no hands!");  

        message_const(MSG_YOU_HAVE_NO_FREE_HANDS);
        You("have no free %s.", body_part(HAND));

            char name[BUFSZ];
            Monnam(name, BUFSZ, u.ustuck);
            pline("%s interferes.", name);
            message_monster(MSG_MONSTER_INTERFERES, u.ustuck);
            mstatusline(u.ustuck);

                You_hear("faint splashing.");
                message_const(MSG_YOU_HEAR_FAINT_SPLASHING);

                message_string(MSG_YOU_CANNOT_REACH_THE_DUNGEON,
                        (u.dz > 0) ? surface(u.ux,u.uy) : ceiling(u.ux,u.uy));
                You_cant("reach the %s.", );

                message_const(MSG_YOU_HEAR_CRACKLING_OF_HELLFIRE);
                You_hear("the crackling of hellfire.");

                message_string(MSG_DUNGEON_SEEMS_HEALTHY_ENOUGH, surface(u.ux,u.uy));
                pline_The("%s seems healthy enough.", surface(u.ux,u.uy));

            message_const(MSG_YOU_HEAR_YOUR_HEART_BEAT);
            You_hear("your heart beat.");

        message_const(MSG_YOU_HEAR_FAINT_TYPING_NOISE);
        You_hear("a faint typing noise.");

        message_const(MSG_THE_INVISIBLE_MONSTER_MOVED);
        pline_The("invisible monster must have moved.");

        You("hear nothing special.");       // not You_hear()
        message_const(MSG_YOU_HEAR_NOTHING_SPECIAL);

MSG_WELDS_TO_YOUR_HAND:
                const char *tmp = xname(wep), *thestr = "The ";
                if (strncmp(tmp, thestr, 4) && !strncmp(The(tmp),thestr,4))
                    tmp = thestr;
                else
                    tmp = "";


                pline("%s%s %s to your %s!", tmp, aobjnam(wep, "weld"),
                        (wep->quan == 1L) ? "itself" : "themselves",
                        bimanual(wep) ?
                                (const char *)makeplural(body_part(HAND))
                                : body_part(HAND));

*/

size_t nh_strlcpy(char *dest, const char *source, size_t dest_size) {
    char *d = dest;
    const char *s = source;
    size_t n = dest_size;

    // copy as many bytes as will fit
    if (n != 0 && --n != 0) {
        do {
            if ((*d++ = *s++) == 0)
                break;
        } while (--n != 0);
    }

    // not enough room in dest, add \0 and traverse rest of source
    if (n == 0) {
        if (dest_size != 0)
            *d = '\0';
        while (*s++)
            continue;
    }

    // count does not include \0
    return s - source - 1;
}

size_t nh_vslprintf(char *dest, size_t dest_size, char *format, va_list ap) {
    size_t n = (dest_size >= 1) ? (dest_size - 1) : 0;
    int ret = vsnprintf(dest, n, format, ap);
    assert(ret >= 0);
    dest[(ret > n) ? n : ret] = 0;
    return ret;
}

size_t nh_slprintf(char *dest, size_t dest_size, char *format, ...) {
    va_list ap;  
    va_start(ap, format);
    int ret = nh_vslprintf(dest, dest_size, format, ap);
    va_end(ap);
    return ret;
}

/*

MSG_YOU_DISRUPT, You("disrupt %s!", name);
MSG_A_HUGE_HOLE_OPENS_UP, "A huge hole opens up..."
MSG_MONSTER_TURNS_INVISIBLE, "%s turns transparent!"
MSG_ENGULFER_OPENS_ITS_MOUTH:
                            if (Blind) {
                                You_feel("a sudden rush of air!");
                            } else {
                                char name[BUFSZ];
                                Monnam(name, BUFSZ, mtmp);
                                pline("%s opens its mouth!", name);
                            }
MSG_MONSTER_LOOKS_BETTER, "%s looks better."
MSG_MONSTER_LOOKS_MUCH_BETTER, "%s looks much better."
MSG_GOLEM_TURNS_TO_FLESH, "%s turns to flesh!"
MSG_GOLEM_LOOKS_FLESHY, "%s looks rather fleshy for a moment."
MSG_MONSTER_LOOKS_WEAKER, "%s suddenly seems weaker!"
MSG_MONSTER_IS_NOT_CARRYING_ANYTHING, "%s is not carrying anything."
MSG_DRAWN_INTO_FORMER_BODY, "%s is suddenly drawn into its former body!"
MSG_A_MONSTER_SUDDENLY_APPEARS, "%s suddenly appears!" % Amonnam()
MSG_POLYPILE_CREATES_GOLEM, "Some %(material)sobjects meld, and %(a_monnam)s arises from the pile!"
    material = {
        PM_IRON_GOLEM: "metal ";
        PM_STONE_GOLEM: "lithic ";
        PM_CLAY_GOLEM: "lithic ";
        PM_FLESH_GOLEM: "organic ";
        PM_WOOD_GOLEM: "wood ";
        PM_LEATHER_GOLEM: "leather ";
        PM_ROPE_GOLEM: "cloth ";
        PM_SKELETON: "bony ";
        PM_GOLD_GOLEM: "gold ";
        PM_GLASS_GOLEM: "glassy ";
        PM_PAPER_GOLEM: "paper ";
        PM_STRAW_GOLEM: "";
    }
MSG_SHOP_KEEPER_GETS_ANGRY, "%s gets angry!"
*/
