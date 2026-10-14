"""Render an original, quiet piano-like ambient loop for the login screen."""
from array import array
from pathlib import Path
import math
import wave

rate = 22050
duration = 36
track = array('f', [0]) * (rate * duration)
def note(midi, start, length, gain):
    frequency = 440 * 2 ** ((midi - 69) / 12)
    first = int(start * rate)
    for offset in range(min(int(length * rate), len(track) - first)):
        t = offset / rate
        envelope = (1 - math.exp(-t * 65)) * math.exp(-t / 1.8) * min(1, (length-t)/0.4)
        tone = math.sin(2*math.pi*frequency*t) + .20*math.sin(2*math.pi*frequency*2*t)*math.exp(-t*2) + .08*math.sin(2*math.pi*frequency*3*t)*math.exp(-t*3)
        track[first+offset] += gain * envelope * tone

chords = [(48,55,60,64,67),(45,52,57,60,64),(41,48,53,57,60),(43,50,55,59,62)]
melody = [72,76,79,76,74,72,67,72,69,72,76,72,71,67,64,67]
for bar, chord in enumerate(chords * 2):
    start = 1 + bar * 4
    for step in range(8):
        note(chord[1+(step%4)], start+step*.5, 3, .10)
    note(chord[0], start, 4, .12)
    note(melody[bar*2],start+.25,3,.11)
    note(melody[bar*2+1],start+2.25,3,.11)
# Light echoes create space without an external audio dependency.
for delay, gain in [(int(rate*.27),.18),(int(rate*.51),.08)]:
    original = array('f',track)
    for index in range(delay,len(track)):
        track[index] += original[index-delay]*gain
peak = max(abs(value) for value in track)
pcm = array('h')
for index, value in enumerate(track):
    t = index/rate
    fade = min(1,t/1.5,(duration-t)/3)
    pcm.append(int(value/peak*.75*fade*32767))
target = Path(__file__).resolve().parents[1]/'assets/music/mist.wav'
target.parent.mkdir(parents=True,exist_ok=True)
with wave.open(str(target),'wb') as out:
    out.setnchannels(1);out.setsampwidth(2);out.setframerate(rate);out.writeframes(pcm.tobytes())
print('Created original 36-second login music: assets/music/mist.wav')
