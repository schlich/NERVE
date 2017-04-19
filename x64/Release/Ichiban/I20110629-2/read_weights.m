data = importdata('z_regress_weights.txt', ',');
trial_x = data(:,1);
weights_x = data(:,2:end);

data = importdata('z_regress_weights.txt', ',');
trial_z = data(:,1);
weights_z = data(:,2:end);

data = importdata('x_weights_log.txt', ',');
trial_x = data(:,1);
weights_x = data(:,2:end);

data = importdata('z_weights_log.txt', ',');
trial_z = data(:,1);
weights_z = data(:,2:end);

min_x = min(min(weights_x));
min_z = min(min(weights_z));
max_x = max(max(weights_x));
max_z = max(max(weights_z));
extreme = max([abs(min_x), abs(min_z), abs(max_x), abs(max_z)]);

figure
imagesc(weights_x)
caxis([-extreme extreme])
colorbar

figure
imagesc(weights_z)
caxis([-extreme extreme])
colorbar


figure
for i=1:size(weights_z,1)
    clf
    for j=1:size(weights_z,2)
        plot([0 weights_x(i,j)], [0 weights_z(i,j)]); hold on
    end
    pause
end
