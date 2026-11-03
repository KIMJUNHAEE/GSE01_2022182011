"""Independent numerical model of companion/locomotion rules; never builds/runs the game.

The real C++ integration/pose checks are in the user-invoked --verify suite.
"""
from collections import deque
import math


def project(point):
    x, y = point
    return (x-y)*42, (x+y)*21


def add(a, b):
    return a[0]+b[0], a[1]+b[1]


def sub(a, b):
    return a[0]-b[0], a[1]-b[1]


def multiply(a, scale):
    return a[0]*scale, a[1]*scale


def pixels(delta):
    return math.hypot(*project(delta))


class Follower:
    def __init__(self, player=(1.4, 0), position=(0, 0)):
        self.position = position
        self.trail = deque((position, player))
        self.speed = 0
        self.following = False

    def update(self, player, leader_speed, dt, clear=lambda a, b: True):
        if not self.trail or (len(self.trail) < 512 and pixels(sub(self.trail[-1], player)) > .01):
            self.trail.append(player)
        previous = self.position
        remaining = 0
        for point in self.trail:
            remaining += pixels(sub(point, previous))
            previous = point
        if remaining > 64:
            self.following = True
        elif remaining <= 44.1:
            self.following = False
        if not self.following:
            self.speed = 0
            return 0
        gap = 56 if leader_speed > 0 else 44
        target = min(155*1.8, max(0, leader_speed+(remaining-gap)*4))
        if leader_speed == 0:
            target = max(12, target)
        self.speed += (target-self.speed)*(1-math.exp(-dt*10))
        budget = min(self.speed*dt, max(0, remaining-44))
        traveled = 0
        while budget > .0001 and self.trail:
            delta = sub(self.trail[0], self.position)
            length = pixels(delta)
            if length < .001:
                self.trail.popleft()
                continue
            fraction = min(1, budget/length, .06/math.hypot(*delta))
            next_position = add(self.position, multiply(delta, fraction))
            if not clear(self.position, next_position):
                self.speed = 0
                break
            self.position = next_position
            budget -= length*fraction
            traveled += length*fraction
            if fraction >= 1:
                self.trail.popleft()
        return traveled


def verify_follow(dt, direction, sprint):
    world_direction = (direction[0]/84+direction[1]/42,
                       direction[1]/42-direction[0]/84)
    player = (1.4, 0)
    follower = Follower(player)
    speed = 155*(1.5 if sprint else 1)
    stopped = 0
    for frame in range(math.ceil(6/dt)):
        player = add(player, multiply(world_direction, speed*dt))
        traveled = follower.update(player, speed, dt)
        assert traveled <= 155*1.8*dt+.001, "Exceeded movement budget"
        if frame*dt > .5 and traveled < .001:
            stopped += 1
    assert stopped == 0, (dt, direction, sprint, stopped)
    for _ in range(math.ceil(3/dt)):
        follower.update(player, 0, dt)
    gap = pixels(sub(player, follower.position))
    assert 43.9 <= gap <= 44.2, (dt, direction, sprint, gap)
    position = follower.position
    for _ in range(math.ceil(1/dt)):
        follower.update(player, 0, dt)
    assert pixels(sub(position, follower.position)) < .001, "Stop-distance oscillation"


def verify_corners():
    player = (6, 0)
    follower = Follower(player, (-2, 0))
    follower.trail = deque(((-2, -2), (4, -2), player))
    visited = []

    def clear(a, b):
        # Keep clear of the expanded crate x=[0,2], y=[-.5,.5].
        assert not (0 < b[0] < 2 and -.5 < b[1] < .5), "Cut through the crate"
        visited.append(b)
        return True

    for _ in range(300):
        follower.update(player, 0, .05, clear)
    assert pixels(sub(player, follower.position)) <= 44.2
    assert any(y < -1.9 for x, y in visited), "Did not preserve the routed corner"


def verify_locomotion():
    dt = 1/60
    filtered = 0
    stopped_for = 0
    rate = 1
    action = "Idle"
    moving = False
    actions = []
    # Single-frame waypoint pauses must not interrupt a sustained run.
    for frame in range(600):
        displaced = frame % 5 != 4
        response = 1-math.exp(-dt*12)
        if displaced:
            stopped_for = 0
            moving = True
            filtered += (6-filtered)*response
        else:
            stopped_for += dt
            if stopped_for >= .14:
                moving = False
                filtered += (0-filtered)*response
        next_action = "Walk" if moving else "Idle"
        if moving and filtered > 1.95*(.65 if action == "Run" else .85):
            next_action = "Run"
        speed = .62 if next_action == "Walk" else 1.95
        target = min(1.8, max(.65, filtered/speed)) if moving else 1
        rate += (target-rate)*response
        assert .65 <= rate <= 1.800001
        action = next_action
        actions.append(action)
    assert set(actions[60:]) == {"Run"}, "Short pauses restarted idle/run transitions"
    # Crossfade endpoints, midpoint, phase preservation and interruption math.
    phase = .4/1
    new_time = phase*2
    assert math.isclose(new_time, .8)
    alpha = .5*.5*(3-2*.5)
    displayed = 4*(1-alpha)+9*alpha
    assert math.isclose(displayed, 6.5)
    assert math.isclose(displayed*(1-0)+0*0, displayed)


if __name__ == "__main__":
    cases = 0
    for dt in (1/120, 1/60, 1/30, .05):
        for direction in ((1, 0), (0, 1), (2**-.5, 2**-.5)):
            for sprint in (False, True):
                verify_follow(dt, direction, sprint)
                cases += 1
    verify_corners()
    verify_locomotion()
    print(f"PASS numerical model: {cases} follow cases, 20..120Hz, walk/run/stop, speed budget")
    print("PASS numerical model: routed corners, one-frame pause grace, playback cap and blend math")
    print("NOTE: This is an independent mathematical model, not a C++ runtime/rendering test.")
