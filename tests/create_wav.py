import wave, struct
with wave.open('clean.wav', 'wb') as f:
    f.setnchannels(2)
    f.setsampwidth(2)
    f.setframerate(44100)
    data = b'\x00' * (44100 * 2 * 2) # 1 second of silence
    f.writeframes(data)
