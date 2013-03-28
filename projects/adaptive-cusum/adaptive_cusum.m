% Adaptive CUSUM algorithm, as detailed in "Adaptive Quickest Change
% Detection with Unknown Parameter" by C. Li, H. Dai, and H. Li
% For now assume the distribution is gaussian pre- and post-change.
% Assume parameter is the mean.  Let the standard deviations be 1.

% lambda is the parameter under hypothesis zero, and is known.
lambda = 25;

% delta is the jump size which rho_hat_a and rho_hat_b are subject
% to each iteration of the procedure.
delta = 0.1;
epsilon = 1;

% rho can exist on the interval rho_min to rho_max
rho_min = 0;
rho_max = 100;
rho_hat_a = lambda - delta/2;
rho_hat_b = lambda + delta/2;

% Number of iterations to do
NUM_ITERATIONS = 1000;
rho_hat_arr = zeros(NUM_ITERATIONS,1);

% Change occurs at change_start on interval 1 - NUM_ITERATIONS
change_start = floor(1 + (NUM_ITERATIONS - 1) * rand(1,1));

% rho is the parameter under hypothesis one, and is to be estimated 
% with rho_hat.
x = (1:NUM_ITERATIONS);
rho_x = x(change_start:NUM_ITERATIONS);
rho_arr = lambda + 3 * sin(2*pi*0.01*rho_x) .* sin(2*pi*0.005*rho_x);

% Standard CUSUM variables
S_n = 0;
S_n_arr = zeros(NUM_ITERATIONS,1);
minS_n = 0;
b = 7;
decision = 0;
stop_updating_decision_time = 0;

% Begin recursion iteration here
for i = 1:NUM_ITERATIONS
    
% Generate an appropriate observation.
    if (i < change_start)
        obs = normrnd(lambda,1);
    else
        obs = normrnd(rho_arr(i - change_start + 1),1);
    end    
    
% D_k = F(rho_hat_b) - F(rho_hat_a)
% F(rho) = E[LLR], ie the expectation of the log-likelihood ratio
% between the post- and pre-change distributions.
    D_k = -((obs - rho_hat_b)^2)/2 + ((obs - rho_hat_a)^2)/2;

% Recursion Rule:
    rho_hat_a = rho_hat_a + epsilon * ( D_k );
    rho_hat_b = rho_hat_a + delta;
    
% rho_hat
    rho_hat = rho_hat_a + 0.5 * delta;
    if (rho_hat > rho_max) 
        rho_hat = rho_max;
    elseif (rho_hat < rho_min)
        rho_hat = rho_min;
    end
    rho_hat_arr(i) = rho_hat;
    
% Now do CUSUM normally, using rho_hat

% Find new statistic
    z_k =  -((obs-rho_hat)^2)/2 + ((obs-lambda)^2)/2;
    % Insignificant stats can be ignored to actually improve the 
    % performance of the algorithm.
    if (abs(z_k) < 2)
        z_k = 0;
    end
    
% Yk is some observation
    S_n = S_n + z_k;

% Check if done
    if ((S_n >= (minS_n + b)) && (stop_updating_decision_time == 0))
        decision = 1;
        change_detected = i;
    elseif S_n < minS_n
        minS_n = S_n;
    end
    if (decision)
        stop_updating_decision_time = 1;
    end
    S_n_arr(i) = S_n;

end

change_start
change_detected
if (change_start > change_detected)
    % False alarm!
else
    time_to_detection = change_detected - change_start
end

% Construct the real progression of the tracked variable.
mean_arr = lambda * ones(1,change_start-1);
mean_arr = [mean_arr rho_arr];

% Compare this with the estimated variable.
figure(1)
subplot(2,1,1)
plot(x, mean_arr, x, rho_hat_arr); title('\rho tracking over time');
xlabel('Samples'); ylabel('Value');grid on;
hold on
plot(change_detected, mean_arr(change_detected),'r.');
hold off

% Compare this with the estimated variable.
subplot(2,1,2)
plot(x, S_n_arr); title('S_n over time');
xlabel('Samples'); ylabel('S_n');grid on;
hold on
plot(change_detected, S_n_arr(change_detected),'r.');
hold off