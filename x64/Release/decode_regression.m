function decode_regression()

readfile = 'C:\Users\Jesse\Desktop\Decoding\x64\Release\features.txt';
writeFileX = 'C:\Users\Jesse\Desktop\Decoding\x64\Release\x_weights.txt';
writeFileZ = 'C:\Users\Jesse\Desktop\Decoding\x64\Release\z_weights.txt';

data = importdata(readfile, ',');

xweights = regress(data(:,1), data(:,3:end));
zweights = regress(data(:,2), data(:,3:end));

save(writeFileX, 'xweights', '-ASCII');
save(writeFileZ, 'zweights', '-ASCII');

close all;

figure(1)
xw = reshape(xweights, 32, 5);
imagesc(xw);
zw = reshape(zweights, 32, 5);
figure(2)
imagesc(zw);



