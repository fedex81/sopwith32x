// Emacs style mode select -*- C++ -*-
//---------------------------------------------------------------------------
//
// $Id: $
//
// Copyright(C) 1984-2000 David L. Clark
// Copyright(C) 2001 Simon Howard
//
// All rights reserved except as specified in the file license.txt.
// Distribution of this file without the license.txt file accompanying
// is prohibited.
//
//---------------------------------------------------------------------------
//
//        swmove   -      SW move all objects and players
//
//---------------------------------------------------------------------------

#include "cgavideo.h"

#include "sw.h"
#include "swasynio.h"
#include "swauto.h"
#include "swcollsn.h"
#include "swdisp.h"
#include "swend.h"
#include "swground.h"
#include "swgrpha.h"
#include "swinit.h"
#include "swmain.h"
#include "swmove.h"
#include "swnetio.h"
#include "swobject.h"
#include "swplanes.h"
#include "swsound.h"
#include "swsymbol.h"
#include "swtitle.h"
#include "swutil.h"

static BOOL quit;

void swmove()
{
	register OBJECTS *ob, *obn;

	if (deltop) {
		delbot->ob_next = objfree;
		objfree = deltop;
		deltop = delbot = NULL;
	}

	if (++dispcnt >= keydelay)
		dispcnt = 0;

	ob = objtop;
	while (ob) {
		obn = ob->ob_next;
		ob->ob_delflg = ob->ob_drwflg;
		ob->ob_oldsym = ob->ob_newsym;
		ob->ob_drwflg = (*ob->ob_movef) (ob);
		if ((playmode == MULTIPLE || playmode == ASYNCH)
		    && ob->ob_index == multbuff->mu_maxplyr
		    && !dispcnt) {
			if (playmode == MULTIPLE)
				multput();
			else
				asynput();
		}
		ob = obn;
	}
	++countmove;

}


static void nearpln(OBJECTS * obp)
{
	register OBJECTS *ob, *obt, *obc;
	register int i, obx, obclr;
	// register int r;

	ob = obp;
	obt = objtop + 1;

	obx = ob->ob_x;
	obclr = ob->ob_owner->ob_clr;

	for (i = 1; obt->ob_type == PLANE; ++i, ++obt) {
		if (obclr == obt->ob_owner->ob_clr)
			continue;

		// sdh: removed use of "equal" function to compare
		// function pointers (why???)

		if (obt->ob_drawf == dispcomp)

			if (playmode != COMPUTER
			    || (obx >= lcompter[i] && obx <= rcompter[i])) {
				obc = compnear[i];
				if (!obc
				    || abs(obx - obt->ob_x)
					< abs(obc->ob_x - obt->ob_x))
					compnear[i] = ob;
			}
	}
}




static int topup(int *counter, int max)
{
	BOOL rc;

	rc = FALSE;
	if (*counter == max)
		return rc;
	if (max < 20) {
		if (!(countmove % 20)) {
			++*counter;
			rc = plyrplane;
		}
	} else {
		*counter += max / 100;
		rc = plyrplane;
	}
	if (*counter > max)
		*counter = max;
	return rc;
}


static void refuel(OBJECTS * obp)
{
	register OBJECTS *ob;

	ob = obp;
	setvdisp();
	if (topup(&ob->ob_life, MAXFUEL))
		dispfgge(ob);
	if (topup(&ob->ob_rounds, MAXROUNDS))
		dispsgge(ob);
	if (topup(&ob->ob_bombs, MAXBOMBS))
		dispbgge(ob);
	if (topup(&ob->ob_missiles, MAXMISSILES))
		dispmgge(ob);
	if (topup(&ob->ob_bursts, MAXBURSTS))
		dispsbgge(ob);
}



static int symangle(OBJECTS * ob)
{
	register int dx, dy;

	dx = ob->ob_dx;
	dy = ob->ob_dy;
	if (dx == 0)
		if (dy < 0)
			return 6;
		else if (dy > 0)
			return 2;
		else
			return 6;
	else if (dx > 0)
		if (dy < 0)
			return 7;
		else if (dy > 0)
			return 1;
		else
			return 0;
	else if (dy < 0)
		return 5;
	else if (dy > 0)
		return 3;
	else
		return 4;
}


BOOL moveplyr(OBJECTS * obp)
{
	register OBJECTS *ob;
	register BOOL rc;
	register int oldx;

	compplane = FALSE;
	plyrplane = TRUE;

	ob = obp;
	currobx = ob->ob_index;

	endstat = endsts[player];

	if (endstat)
		if (--endcount <= 0) {
			if (playmode != MULTIPLE && playmode != ASYNCH
			    && !quit)
				swrestart();
			swend(NULL, YES);
		}

	if (!dispcnt) {
		if (playmode == MULTIPLE)
			multkey = multget(ob);
		else if (playmode == ASYNCH)
			multkey = asynget(ob);
		else {
			// sdh: use the cga (sdl) interface to
			// read key status

			multkey = CGA_GetGameKeys();
		}
		interpret(ob, multkey);
	} else {
		ob->ob_flaps = 0;
		ob->ob_bfiring = ob->ob_bombing = FALSE;
		ob->ob_mfiring = NULL;
	}

	if ((ob->ob_state == CRASHED || ob->ob_state == GHOSTCRASHED)
	    && ob->ob_hitcount <= 0) {

		// sdh: infinite lives in multiplayer mode

		if (playmode != MULTIPLE && playmode != ASYNCH)
			++ob->ob_crashcnt;

		if (endstat != WINNER
		    && (ob->ob_life <= QUIT
			|| (playmode != MULTIPLE && playmode != ASYNCH
			    && ob->ob_crashcnt >= MAXCRASH))) {
			if (!endstat)
				loser(ob);
		} else {
			initplyr(ob);
			initdisp(YES);
			if (endstat == WINNER) {
				if (ctlbreak())
					swend(NULL, YES);
				winner(ob);
			}
		}
	}

	oldx = ob->ob_x;
	rc = movepln(ob);

	if (oldx <= SCR_LIMIT || oldx >= (MAX_X - SCR_LIMIT))
		dispdx = 0;
	else {
		dispdx = ob->ob_x - oldx;
		displx += dispdx;
		disprx += dispdx;
	}

	if (!ob->ob_athome) {
		setvdisp();
		if (ob->ob_firing)
			dispsgge(ob);
		if (ob->ob_bombing)
			dispbgge(ob);
		if (ob->ob_mfiring)
			dispmgge(ob);
		if (ob->ob_bfiring)
			dispsbgge(ob);
	}

	return rc;
}




void interpret(OBJECTS * obp, int key)
{
	register OBJECTS *ob;
	register obstate_t state;

	ob = obp;
	ob->ob_flaps = 0;
	ob->ob_bombing = ob->ob_bfiring = 0;
	ob->ob_mfiring = ob->ob_firing = NULL;

	state = ob->ob_state;

	if (state != FLYING
	    && state != STALLED
	    && state != FALLING
	    && state != WOUNDED
	    && state != WOUNDSTALL
	    && state != GHOST
	    && state != GHOSTSTALLED)
		return;

	if (state != FALLING) {
		if (endstat) {
			if (endstat == LOSER && plyrplane)
				gohome(ob);
			return;
		}

		if (key & K_BREAK) {
			ob->ob_life = QUIT;
			ob->ob_home = FALSE;
			if (ob->ob_athome) {
				ob->ob_state = state = 
				    state >= FINISHED ? GHOSTCRASHED : CRASHED;
				ob->ob_hitcount = 0;
			}
			if (plyrplane)
				quit = TRUE;
		}

		if (key & K_HOME)
			if (state == FLYING || state == GHOST 
			    || state == WOUNDED)
				ob->ob_home = TRUE;
	}

	if ((countmove & 1)
	    || (state != WOUNDED && state != WOUNDSTALL)) {
		if (key & K_FLAPU) {
			++ob->ob_flaps;
			ob->ob_home = FALSE;
		}

		if (key & K_FLAPD) {
			--ob->ob_flaps;
			ob->ob_home = FALSE;
		}

		if (key & K_FLIP) {
			ob->ob_orient = !ob->ob_orient;
			ob->ob_home = FALSE;
		}

		if (key & K_DEACC) {
			if (ob->ob_accel)
				--ob->ob_accel;
			ob->ob_home = FALSE;
		}

		if (key & K_ACCEL) {
			if (ob->ob_accel < MAX_THROTTLE)
				++ob->ob_accel;
			ob->ob_home = FALSE;
		}
	}

	if ((key & K_SHOT) && state < FINISHED)
		ob->ob_firing = ob;

	if ((key & K_MISSILE) && state < FINISHED)
		ob->ob_mfiring = ob;

	if ((key & K_BOMB) && state < FINISHED)
		ob->ob_bombing = TRUE;

	if ((key & K_STARBURST) && state < FINISHED)
		ob->ob_bfiring = TRUE;

	if (key & K_SOUND)
		if (plyrplane) {
			if (soundflg) {
				sound(0, 0, NULL);
				swsound();
			}
			soundflg = !soundflg;
		}

	if (ob->ob_home)
		gohome(ob);
}

BOOL movecomp(OBJECTS * obp)
{
	register OBJECTS *ob;
//      int oldx;
	int rc;

	compplane = TRUE;
	plyrplane = FALSE;

	ob = obp;
	ob->ob_flaps = 0;
	ob->ob_bfiring = ob->ob_bombing = FALSE;
	ob->ob_mfiring = NULL;

	currobx = ob->ob_index;
	endstat = endsts[currobx];

	if (!dispcnt)
		ob->ob_firing = NULL;

	switch (ob->ob_state) {

	case WOUNDED:
	case WOUNDSTALL:
		if (countmove & 1)
			break;

	case FLYING:
	case STALLED:
		if (endstat) {
			gohome(ob);
			break;
		}
		if (!dispcnt)
			swauto(ob);
		break;

	case CRASHED:
		ob->ob_firing = NULL;
		if (ob->ob_hitcount <= 0 && !endstat)
			initcomp(ob);
		break;

	default:
		ob->ob_firing = NULL;
		break;
	}


	rc = movepln(ob);

	return rc;
}



BOOL movepln(OBJECTS * obp)
{
	register OBJECTS *ob = obp;
	register int nangle, nspeed, limit, update;
	obstate_t state, newstate;
	int x, y, stalled;
	// int grv;
	static char gravity[] = { 0, -1, -2, -3, -4, -3, -2, -1,
		0, 1, 2, 3, 4, 3, 2, 1
	};

	state = ob->ob_state;

	switch (state) {
	case FINISHED:
	case WAITING:
		return FALSE;

	case CRASHED:
	case GHOSTCRASHED:
		--ob->ob_hitcount;
		break;

	case FALLING:
		ob->ob_hitcount -= 2;
		if ((ob->ob_dy < 0) && ob->ob_dx) {
			if (ob->ob_orient ^ (ob->ob_dx < 0))
				ob->ob_hitcount -= ob->ob_flaps;
			else
				ob->ob_hitcount += ob->ob_flaps;
		}

		if (ob->ob_hitcount <= 0) {
			if (ob->ob_dy < 0) {
				if (ob->ob_dx < 0)
					++ob->ob_dx;
				else if (ob->ob_dx > 0)
					--ob->ob_dx;
				else
					ob->ob_orient = !ob->ob_orient;
			}

			if (ob->ob_dy > -10)
				--ob->ob_dy;
			ob->ob_hitcount = FALLCOUNT;
		}
		ob->ob_angle = symangle(ob) * 2;
		if (ob->ob_dy <= 0)
			initsound(ob, S_FALLING);
		break;

	case STALLED:
		newstate = FLYING;
		goto commonstall;

	case GHOSTSTALLED:
		newstate = GHOST;
		goto commonstall;

	case WOUNDSTALL:
		newstate = WOUNDED;

	      commonstall:
		stalled = ob->ob_angle != (3 * ANGLES / 4)
			|| ob->ob_speed < gminspeed;
		if (!stalled)
			ob->ob_state = state = newstate;
		goto controlled;

	case FLYING:
	case WOUNDED:
	case GHOST:
		stalled = ob->ob_y >= MAX_Y;
		if (stalled) {
			if (playmode == NOVICE) {
				ob->ob_angle = (3 * ANGLES / 4);
				stalled = FALSE;
			} else {
				stallpln(ob);
				state = ob->ob_state;
			}
		}

	      controlled:
		if (goingsun && plyrplane)
			break;

		if (ob->ob_life <= 0 && !ob->ob_athome
		    && (state == FLYING || state == STALLED
			|| state == WOUNDED
			|| state == WOUNDSTALL)) {
			hitpln(ob);
			scorepln(ob);
			return movepln(ob);
		}

		if (ob->ob_firing)
			initshot(ob, NULL);

		if (ob->ob_bombing)
			initbomb(ob);

		if (ob->ob_mfiring)
			initmiss(ob);

		if (ob->ob_bfiring)
			initburst(ob);

		nangle = ob->ob_angle;
		nspeed = ob->ob_speed;
		update = ob->ob_flaps;

		if (update) {
			if (ob->ob_orient)
				nangle -= update;
			else
				nangle += update;
			nangle = (nangle + ANGLES) % ANGLES;
		}

		if (!(countmove & 0x0003)) {
			if (!stalled && nspeed < gminspeed
			    && playmode != NOVICE) {
				--nspeed;
				update = TRUE;
			} else {
				limit = gminspeed
				    + ob->ob_accel + gravity[nangle];
				if (nspeed < limit) {
					++nspeed;
					update = TRUE;
				} else if (nspeed > limit) {
					--nspeed;
					update = TRUE;
				}
			}
		}

		if (update) {
			if (ob->ob_athome)
				if (ob->ob_accel || ob->ob_flaps)
					nspeed = gminspeed;
				else
					nspeed = 0;

			else if (nspeed <= 0 && !stalled) {
				if (playmode == NOVICE)
					nspeed = 1;
				else {
					stallpln(ob);
					return movepln(ob);
				}
			}

			ob->ob_speed = nspeed;
			ob->ob_angle = nangle;

			if (stalled) {
				ob->ob_dx = ob->ob_ldx = ob->ob_ldy = 0;
				ob->ob_dy = -nspeed;
			} else
				setdxdy(ob,
					nspeed * COS(nangle),
					nspeed * SIN(nangle));
		}

		if (stalled) {
			if (!--ob->ob_hitcount) {
				ob->ob_orient = !ob->ob_orient;
				ob->ob_angle = ((3 * ANGLES / 2)
						- ob->ob_angle)
				    % ANGLES;
				ob->ob_hitcount = STALLCOUNT;
			}
		}

		if (!compplane) {
			if (plyrplane
			    && ob->ob_speed >
				(ob->ob_life % (MAXFUEL / 10))) {
				setvdisp();
				dispfgge(ob);
			}
			ob->ob_life -= ob->ob_speed;
		}

		if (ob->ob_speed)
			ob->ob_athome = FALSE;
		break;
	default:
		break;
	}

	if (endstat == WINNER && plyrplane && goingsun)
		ob->ob_newsym = swwinsym[endcount / 18];
	else
		ob->ob_newsym = ob->ob_state == FINISHED
		    ? NULL : ((ob->ob_state == FALLING
			       && !ob->ob_dx && ob->ob_dy < 0)
			      ? swhitsym[ob->ob_orient]
			      : swplnsym[ob->ob_orient][ob->ob_angle]);
	movexy(ob, &x, &y);

	if (x < 0)
		x = ob->ob_x = 0;
	else if (x >= (MAX_X - 16))
		x = ob->ob_x = MAX_X - 16;

	if (!compplane
	    && (ob->ob_state == FLYING
		|| ob->ob_state == STALLED
		|| ob->ob_state == WOUNDED
		|| ob->ob_state == WOUNDSTALL)
	    && !endsts[player])
		nearpln(ob);

	deletex(ob);
	insertx(ob, ob->ob_xnext);

	if (ob->ob_bdelay)
		--ob->ob_bdelay;
	if (ob->ob_mdelay)
		--ob->ob_mdelay;
	if (ob->ob_bsdelay)
		--ob->ob_bsdelay;

	if (!compplane && ob->ob_athome && ob->ob_state == FLYING)
		refuel(ob);

	if (y < MAX_Y && y >= 0) {
		if (ob->ob_state == FALLING
		    || ob->ob_state == WOUNDED
		    || ob->ob_state == WOUNDSTALL)
			initsmok(ob);
		setvdisp();
		dispwobj(ob);
		return plyrplane || ob->ob_state < FINISHED;
	}

	return FALSE;
}



static void adjustfall(OBJECTS * obp)
{
	register OBJECTS *ob;

	ob = obp;
	if (!--ob->ob_life) {
		if (ob->ob_dy < 0) {
			if (ob->ob_dx < 0)
				++ob->ob_dx;
			else if (ob->ob_dx > 0)
				--ob->ob_dx;
		}
		if (ob->ob_dy > -10)
			--ob->ob_dy;
		ob->ob_life = BOMBLIFE;
	}
}


BOOL moveshot(OBJECTS * obp)
{
	register OBJECTS *ob;
	int x, y;

	ob = obp;
	deletex(ob);
	if (!--ob->ob_life) {
		deallobj(ob);
		return FALSE;
	}

	movexy(ob, &x, &y);

	if (y >= MAX_Y || y <= (int) ground[x]
	    || x < 0 || x >= MAX_X) {
		deallobj(ob);
		return FALSE;
	}

	insertx(ob, ob->ob_xnext);
	ob->ob_newsym = (char *) 0x83;
	return TRUE;
}



BOOL movebomb(OBJECTS * obp)
{
	register OBJECTS *ob;
	int x, y;

	ob = obp;

	deletex(ob);

	if (ob->ob_life < 0) {
		deallobj(ob);
		ob->ob_state = FINISHED;
		setvdisp();
		dispwobj(ob);
		return FALSE;
	}

	adjustfall(ob);

	if (ob->ob_dy <= 0)
		initsound(ob, S_BOMB);

	movexy(ob, &x, &y);

	if (y < 0 || x < 0 || x >= MAX_X) {
		deallobj(ob);
		stopsound(ob);
		ob->ob_state = FINISHED;
		setvdisp();
		dispwobj(ob);
		return FALSE;
	}

	ob->ob_newsym = swbmbsym[symangle(ob)];
	insertx(ob, ob->ob_xnext);

	if (y >= MAX_Y)
		return FALSE;

	setvdisp();
	dispwobj(ob);
	return TRUE;
}



BOOL movemiss(OBJECTS * obp)
{
	register OBJECTS *ob;
	int x, y, angle;
	OBJECTS *obt;

	ob = obp;

	deletex(ob);

	if (ob->ob_life < 0) {
		deallobj(ob);
		ob->ob_state = FINISHED;
		setvdisp();
		dispwobj(ob);
		return FALSE;
	}

	if (ob->ob_state == FLYING) {
		obt = ob->ob_target;

		if (obt != ob->ob_owner && (ob->ob_life & 1)) {
			if (obt->ob_target)
				obt = obt->ob_target;
			aim(ob, obt->ob_x, obt->ob_y, NULL, NO);
			angle = ob->ob_angle
			    =
			    (ob->ob_angle + ob->ob_flaps +
			     ANGLES) % ANGLES;
			setdxdy(ob, 
				ob->ob_speed * COS(angle),
				ob->ob_speed * SIN(angle));
		}
		movexy(ob, &x, &y);
		if (!--ob->ob_life || y >= ((MAX_Y * 3) / 2)) {
			ob->ob_state = FALLING;
			++ob->ob_life;
		}
	} else {
		adjustfall(ob);
		ob->ob_angle = (ob->ob_angle + 1) % ANGLES;
		movexy(ob, &x, &y);
	}

	if (y < 0 || x < 0 || x >= MAX_X) {
		deallobj(ob);
		ob->ob_state = FINISHED;
		setvdisp();
		dispwobj(ob);
		return FALSE;
	}

	ob->ob_newsym = swmscsym[ob->ob_angle];
	insertx(ob, ob->ob_xnext);

	if (y >= MAX_Y)
		return FALSE;

	setvdisp();
	dispwobj(ob);
	return TRUE;
}



BOOL moveburst(OBJECTS * obp)
{
	register OBJECTS *ob;
	int x, y;

	ob = obp;
	deletex(ob);
	if (ob->ob_life < 0) {
		ob->ob_owner->ob_target = NULL;
		deallobj(ob);
		return FALSE;
	}

	adjustfall(ob);
	movexy(ob, &x, &y);

	if (y <= (int) ground[x] || x < 0 || x >= MAX_X) {
		ob->ob_owner->ob_target = NULL;
		deallobj(ob);
		return FALSE;
	}

	ob->ob_owner->ob_target = ob;
	ob->ob_newsym = swbstsym[ob->ob_life & 1];
	insertx(ob, ob->ob_xnext);

	return y < MAX_Y;
}




BOOL movetarg(OBJECTS * obt)
{
	int r;
	register OBJECTS *obp, *ob;

	ob = obt;
	obp = objtop;
	ob->ob_firing = NULL;
	if (gamenum 
	    && ob->ob_state == STANDING
	    && (obp->ob_state == FLYING
		|| obp->ob_state == STALLED
		|| obp->ob_state == WOUNDED
		|| obp->ob_state == WOUNDSTALL)
	    && ob->ob_clr != obp->ob_clr
	    && (gamenum > 1 || (countmove & 0x0001))
	    && ((r = range(ob->ob_x, ob->ob_y, obp->ob_x, obp->ob_y)) > 0)
	    && r < targrnge)
		initshot(ob, ob->ob_firing = obp);

	if (--ob->ob_hitcount < 0)
		ob->ob_hitcount = 0;

	ob->ob_newsym = ob->ob_state == STANDING
	    ? swtrgsym[ob->ob_orient]
	    : swhtrsym;
	return TRUE;
}



BOOL moveexpl(OBJECTS * obp)
{
	register OBJECTS *ob;
	int x, y;
	register int orient;

	ob = obp;
	orient = ob->ob_orient;
	deletex(ob);
	if (ob->ob_life < 0) {
		if (orient)
			stopsound(ob);
		deallobj(ob);
		return FALSE;
	}

	if (!--ob->ob_life) {
		if (ob->ob_dy < 0) {
			if (ob->ob_dx < 0)
				++ob->ob_dx;
			else if (ob->ob_dx > 0)
				--ob->ob_dx;
		}
		if ((ob->ob_orient && ob->ob_dy > -10)
		    || (!ob->ob_orient && ob->ob_dy > -gminspeed))
			--ob->ob_dy;
		ob->ob_life = EXPLLIFE;
	}

	movexy(ob, &x, &y);

	if (y <= (int) ground[x]
	    || x < 0 || x >= MAX_X) {
		if (orient)
			stopsound(ob);
		deallobj(ob);
		return (FALSE);
	}
	++ob->ob_hitcount;

	insertx(ob, ob->ob_xnext);
	ob->ob_newsym = swexpsym[ob->ob_orient];

	return y < MAX_Y;
}



BOOL movesmok(OBJECTS * obp)
{
	register OBJECTS *ob;
	register obstate_t state;

	ob = obp;

	state = ob->ob_owner->ob_state;

	if (!--ob->ob_life
	    || (state != FALLING
		&& state != WOUNDED
		&& state != WOUNDSTALL
		&& state != CRASHED)) {
		deallobj(ob);
		return FALSE;
	}
	ob->ob_newsym = (char *) (0x80 + ob->ob_clr);

	return TRUE;
}



BOOL moveflck(OBJECTS * obp)
{
	register OBJECTS *ob;
	int x, y;

	ob = obp;
	deletex(ob);

	if (ob->ob_life == -1) {
		setvdisp();
		dispwobj(ob);
		deallobj(ob);
		return FALSE;
	}

	if (!--ob->ob_life) {
		ob->ob_orient = !ob->ob_orient;
		ob->ob_life = FLOCKLIFE;
	}

	if (ob->ob_x < MINFLCKX || ob->ob_x > MAXFLCKX)
		ob->ob_dx = -ob->ob_dx;

	movexy(ob, &x, &y);
	insertx(ob, ob->ob_xnext);
	ob->ob_newsym = swflksym[ob->ob_orient];
	setvdisp();
	dispwobj(ob);
	return TRUE;
}



BOOL movebird(OBJECTS * obp)
{
	register OBJECTS *ob;
	int x, y;

	ob = obp;

	deletex(ob);

	if (ob->ob_life == -1) {
		deallobj(ob);
		return FALSE;
	} else if (ob->ob_life == -2) {
		ob->ob_dy = -ob->ob_dy;
		ob->ob_dx = (countmove & 7) - 4;
		ob->ob_life = BIRDLIFE;
	} else if (!--ob->ob_life) {
		ob->ob_orient = !ob->ob_orient;
		ob->ob_life = BIRDLIFE;
	}

	movexy(ob, &x, &y);

	insertx(ob, ob->ob_xnext);
	ob->ob_newsym = swbrdsym[ob->ob_orient];
	if (y >= MAX_Y || y <= (int) ground[x]
	    || x < 0 || x >= MAX_X) {
		ob->ob_y -= ob->ob_dy;
		ob->ob_life = -2;
		return FALSE;
	}
	return TRUE;
}




BOOL moveox(OBJECTS * ob)
{
	ob->ob_newsym = swoxsym[ob->ob_state != STANDING];
	return TRUE;
}




BOOL crashpln(OBJECTS * obp)
{
	register OBJECTS *ob, *obo;

	ob = obp;

	if (ob->ob_dx < 0)
		ob->ob_angle = (ob->ob_angle + 2) % ANGLES;
	else
		ob->ob_angle = (ob->ob_angle + ANGLES - 2) % ANGLES;

	ob->ob_state = ob->ob_state >= GHOST ? GHOSTCRASHED : CRASHED;
	ob->ob_athome = FALSE;
	ob->ob_dx = ob->ob_dy = ob->ob_ldx = ob->ob_ldy = ob->ob_speed = 0;

	obo = &oobjects[ob->ob_index];
	ob->ob_hitcount = ((abs(obo->ob_x - ob->ob_x) < SAFERESET)
			   && (abs(obo->ob_y - ob->ob_y) < SAFERESET))
	    ? (MAXCRCOUNT << 1) : MAXCRCOUNT;

	return TRUE;
}



BOOL hitpln(OBJECTS * obp)
{
	register OBJECTS *ob;

	ob = obp;
	ob->ob_ldx = ob->ob_ldy = 0;
	ob->ob_hitcount = FALLCOUNT;
	ob->ob_state = FALLING;
	ob->ob_athome = FALSE;

	return TRUE;
}



BOOL stallpln(OBJECTS * obp)
{
	register OBJECTS *ob;

	ob = obp;
	ob->ob_ldx = ob->ob_ldy = ob->ob_orient = ob->ob_dx = 0;
	ob->ob_angle = 7 * ANGLES / 8;
	ob->ob_speed = 0;
	ob->ob_dy = 0;
	ob->ob_hitcount = STALLCOUNT;
	ob->ob_state = 
		ob->ob_state >= GHOST ? GHOSTSTALLED :
		ob->ob_state == WOUNDED ? WOUNDSTALL : STALLED;
	ob->ob_athome = FALSE;

	return TRUE;
}




BOOL insertx(OBJECTS * ob, OBJECTS * obp)
{
	register OBJECTS *obs;
	register int obx;

	obs = obp;
	obx = ob->ob_x;
	if (obx < obs->ob_x)
		do {
			obs = obs->ob_xprev;
		} while (obx < obs->ob_x);
	else {
		while (obx >= obs->ob_x)
			obs = obs->ob_xnext;
		obs = obs->ob_xprev;
	}
	ob->ob_xnext = obs->ob_xnext;
	ob->ob_xprev = obs;
	obs->ob_xnext->ob_xprev = ob;
	obs->ob_xnext = ob;

	return TRUE;
}



void deletex(OBJECTS * obp)
{
	register OBJECTS *ob;

	ob = obp;
	ob->ob_xnext->ob_xprev = ob->ob_xprev;
	ob->ob_xprev->ob_xnext = ob->ob_xnext;
}



//---------------------------------------------------------------------------
//
// $Log: $
//
// sdh 21/10/2001: use new obtype_t and obstate_t
// sdh 21/10/2001: reformatted with indent. edited some code by hand to
//                 make it more readable
// sdh 19/10/2001: removed all externs, these are now in headers
//                 shuffled some functions around to shut up compiler
// sdh 18/10/2001: converted all functions in this file to ANSI-style arguments
//
// 87-04-09        Delay between starbursts.
// 87-04-04        Missile and starburst support.
// 87-04-01        Missiles.
// 87-03-31        Allow wounded plane to fly home
// 87-03-30        Novice Player
// 87-03-12        Computer plane heads home at end.
// 87-03-12        Prioritize bombs/shots over flaps.
// 87-03-12        Proper ASYCHRONOUS end of game.
// 87-03-12        Crashed planes stay longer at home.
// 87-03-12        Wounded airplanes.
// 87-03-09        Microsoft compiler.
// 85-10-31        Atari
// 84-02-07        Development
//
//---------------------------------------------------------------------------


