from scipy.io import wavfile
samplerate, data = wavfile.read('./loop.wav')
print(samplerate)
print(data)

min_data=min(data)
max_data=max(data)
max_abs=max(abs(min_data), abs(max_data))
print(max_abs)
# shift data
scale_factor=(2*max_abs)/4096
shifted=list(map(lambda x: int((x + max_abs)/scale_factor), data))

output=[]
for i in shifted:
	output.append(str(i))
	output.append(str(4096 - i))

o=','.join(output)
file = open('loop.c', 'w+')
file.write('const uint16_t loop['+str(len(output))+'] = {\n')
file.write(o)
file.write('\n};\n')
