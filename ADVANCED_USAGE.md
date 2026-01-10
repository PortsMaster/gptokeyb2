# Advanced gptokeyb2 Usage

`gptokeyb2` was written with the idea of it running in a state machine. Wheras the old one was fixed keybindings the new one supports multiple control states which can be moved between.

## Hotkey

Original `gptokeyb` support `_hk` suffix for keys. This allowed you to hold the `hotkey` and press the key for an additional keybinding.

This can be replicated in `gptokeyb2` by writing a hotkey state, the example below is the .gptk file from Augustus:

```ini
back = esc
start = p                   # Pause

a = mouse_left
b = mouse_right
x = q                       # Clone building under mouse cursor

l1_hk = u                   # Decrease game speed
l2 = end                    # Rotate counter clock wise
l2_hk = f1                  # Save Quicksave
l2_hk = add_ctrl

r1 = mouse_slow
r1_hk = +                   # Increase game speed

r2 = home                   # Rotate clock wise
r2_hk = f1                  # Load Quicksave

up = up
down = down
left = left
right = right

left_analog_up = mouse_movement_up
left_analog_down = mouse_movement_down
left_analog_left = mouse_movement_left
left_analog_right = mouse_movement_right

right_analog_up = up
right_analog_up = repeat
right_analog_down = down
right_analog_down = repeat
right_analog_left = left
right_analog_left = repeat
right_analog_right = right
right_analog_right = repeat

deadzone_triggers = 3000
mouse_scale = 6114
mouse_delay = 16

deadzone_mode = axial
deadzone = 1000
deadzone_scale = 7
deadzone_delay = 16
```

In `gptokeyb2` this can be achieved by writing.

```ini
[config]
repeat_delay = 500
repeat_rate = 60
mouse_slow_scale = 50
deadzone_mode = axial
deadzone_scale = 7
deadzone_x = 1000
deadzone_y = 1000
deadzone_triggers = 3000
dpad_mouse_normalize = true

[controls]
a = "mouse_left"
b = "mouse_right"
x = "q"                      # Clone building under mouse cursor
y =

l1 =
l2 = "end"                   # Rotate counter clock wise
l3 =

r1 = mouse_slow 
r2 = "home"                  # Rotate clock wise
r3 =

start = "p"                  # Pause
back = "esc"
guide =

dpad         = mouse_movement
left_analog  = mouse_movement
right_analog = arrow_keys repeat

hotkey = hold_state hotkey

[controls:hotkey]

l1 = u                      # Decrease game speed
l2 = f1 add_ctrl            # Save Quicksave

r1 = +                      # Increase game speed
r2 = f1                     # Load Quicksave
```

## Super Hotkeys

Ever needed an abundance of buttons for a roguelike?

In the example below you can see that abxy has different bindings using `hold_state` layering. With that you can create keybindings for different buttons depending on whether the `l2`, `r2`, or `l2` & `r2` are held down.

```ini
[controls]
overlay = clear

x = i
b = k
y = j
a = l

l2 = hold_state hotkey1
r2 = hold_state hotkey2

[controls:hotkey1]
overlay = parent

r2 = hold_state hotkey_both

x = g
b = h
y = y
a = t

[controls:hotkey2]
overlay = parent

l2 = hold_state hotkey_both

x = e
b = 6
y = 7
a = 8

[controls:hotkey_both]
overlay = parent

x = p
b = o
y = m
a = n
```

Here it is broken down:

1. **Initial Controls**:
   - The initial configuration defines the mappings for the gamepad buttons to keyboard keys.
   - `l2` and `r2` are mapped to switch between different states (`hotkey1`, `hotkey2`, and `hotkey_both`).
   - `x`, `b`, `y`, and `a` are mapped to the keys `i`, `k`, `j`, and `l` respectively.

2. **State: `hotkey1`**:
   - This state defines a sub-layer where the mappings change when `hotkey1` is held.
   - The `overlay` directive specifies that this state inherits mappings from the parent state (`clear` in this case).
   - The mappings for `x`, `b`, `y`, and `a` change to `g`, `h`, `y`, and `t` respectively when `hotkey1` is held.
   - `r2` changes the state to `hotkey_both` when held.

3. **State: `hotkey2`**:
   - Similar to `hotkey1`, this state defines a sub-layer with different mappings when `hotkey2` is held.
   - Again, it inherits mappings from the parent state.
   - The mappings for `x`, `b`, `y`, and `a` change to `e`, `6`, `7`, and `8` respectively when `hotkey2` is held.
   - `l2` changes the state to `hotkey_both` when held.

4. **State: `hotkey_both`**:
   - This state represents a situation where both `hotkey1` and `hotkey2` are held simultaneously.
   - It inherits mappings from the parent state.
   - The mappings for `x`, `b`, `y`, and `a` change to `p`, `o`, `m`, and `n` respectively when both `hotkey1` and `hotkey2` are held.

## Word sets

It is possible to have text input in the game for naming save game files or entering cheats.


```
[config]
wordset = cheats "enable_cheats" "set god" "unset god"

[controls]
overlay = clear

start = hold_state word_input
back = hold_state text_input

[controls:word_input]
overlay = clear

wordset = cheats
right_analog_up = "f1" ; console
right_analog_left = "prev_word"
right_analog_right = "next_word"
right_analog_down = "finish_text"

[controls:text_input]
# charset = and wordset = can't both be set in the same controls group
charset = extended

start = pop_state
up = prev_letter
down = next_letter
right = add_letter
left = "backspace"
a = "enter"
b = "backspace"

```