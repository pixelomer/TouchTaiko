# TouchTaiko

Program based on evtest for playing osu!taiko (or a similar Taiko game) using the touchpad. The touchpad will function in a way similar to touch controls osu!taiko: Touching the outer areas of the touchpad will send ka inputs and touching the inner areas will send don inputs. Pressing down (i.e. clicking) isn't required.

## Usage

Determine your keyboard and touchpad devices using `sudo evtest`. Then,

```
usage: ./touchtaiko <touchpad> <keyboard>
```

For example:

```bash
$ sudo ./touchtaiko /dev/input/event7 /dev/input/event4
```
