%% Smart Parking Lot System - Glavna datoteka

function SmartParkingGUI()
    % Kreiranje glavne figure
    fig = uifigure('Position', [100 100 1200 700], ...
                   'Name', 'Smart Parking Lot System', ...
                   'Color', [0.9 0.9 0.9]);
    
    % Naslov
    titleLabel = uilabel(fig);
    titleLabel.Text = 'PAMETNO PARKIRALIŠTE';
    titleLabel.FontSize = 24;
    titleLabel.FontWeight = 'bold';
    titleLabel.FontColor = [0 0.3 0.6];
    titleLabel.Position = [400 650 400 40];
    titleLabel.HorizontalAlignment = 'center';
    
    % Kreiramo objekat parking sistema
    parkingSystem = ParkingSystem(fig);
    
    % Dodajemo dugmad za kontrolu
    createControlButtons(fig, parkingSystem);
end

function createControlButtons(fig, system)
    % Panel za dugmad
    controlPanel = uipanel(fig, 'Position', [50 50 1100 100], ...
                          'Title', 'Kontrole', 'FontSize', 12);
    
    % Dugmad za osnovne funkcije
    btnNames = {'PARKIRAJ VOZILO', 'NAPLATA', 'DODAJ PRETPLATNIKA', ...
                'GENERIŠI IZVEŠTAJ', 'SIMULIRAJ LPR', 'HITNI SISTEM'};
    btnCallbacks = {@parkVehicle, @processPayment, @addSubscriber, ...
                    @generateReport, @simulateLPR, @emergencySystem};
    
    for i = 1:length(btnNames)
        btn = uibutton(controlPanel, 'push');
        btn.Text = btnNames{i};
        btn.Position = [50 + (i-1)*170, 20, 150, 40];
        btn.FontSize = 10;
        btn.FontWeight = 'bold';
        
        % Postavljanje callback funkcija
        switch i
            case 1
                btn.ButtonPushedFcn = @(~,~) parkVehicleBtn(system);
            case 2
                btn.ButtonPushedFcn = @(~,~) paymentBtn(system);
            case 3
                btn.ButtonPushedFcn = @(~,~) addSubscriberBtn(system);
            case 4
                btn.ButtonPushedFcn = @(~,~) reportBtn(system);
            case 5
                btn.ButtonPushedFcn = @(~,~) lprBtn(system);
            case 6
                btn.ButtonPushedFcn = @(~,~) emergencyBtn(system);
        end
    end
end

%% Funkcije za dugmad
function parkVehicleBtn(system)
    system.parkVehicle();
end

function paymentBtn(system)
    system.processPayment();
end

function addSubscriberBtn(system)
    system.addMonthlySubscriber();
end

function reportBtn(system)
    system.generateReport();
end

function lprBtn(system)
    system.simulateLicensePlateRecognition();
end

function emergencyBtn(system)
    system.emergencyEvacuation('Fire');
end