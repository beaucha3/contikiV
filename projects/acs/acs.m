%% Initialization Stuff
clear all;
close all;
kill_port();
port = start_port('/dev/ttyUSB0');

runtime = 500;
PREC = 2048;        % Precision of Wn
threshold = 10;

Wn = zeros(1,runtime);
acc = zeros(1,runtime);

counter = 0;

figure(1);
subplot(2,1,1);
title('Significant Statistic, W_n');
xlabel('Sample');
ylabel('W_n');
subplot(2,1,2);
title('Acceleration');
xlabel('Sample');
ylabel('a');


% We are using an ADXL327 with an MSP430 12-bit ADC, Vcc = 3V, which has a 
% sensitivity of 516 - 574(typ.) - 631 per g.

%% Main While Loop
tic
while(counter <= runtime)
    
    while(port.BytesAvailable<1)
        % Wait for data to come in.
    end
    
    % Read data from serial port
    while( port.BytesAvailable > 0 )
        d = fscanf(port,'%d,%d\n');
        if( length(d) == 2 )
            counter = counter + 1;
            Wn(counter) =  d(1)/PREC;
            acc(counter) = d(2);
        end
    end
    
    figure(1);
    subplot(2,1,1);
    hold on;
    line([0 counter], [threshold threshold], 'Color', 'm');   % Threshold
    plot( 1:counter, Wn(1:counter) );
    hold off;
    
    subplot(2,1,2);
    hold on;
    line([0 counter], [3547 3547], 'Color', 'c');   % Pre change mean
    line([0 counter], [3157 3157], 'Color', 'r');   % Post change mean
    plot( 1:counter, acc(1:counter) );
    hold off;
    
    drawnow;
    
end
toc