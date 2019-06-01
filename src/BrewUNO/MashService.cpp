#include <BrewUNO/MashService.h>

MashService::MashService(FS *fs, TemperatureService *temperatureService, Pump *pump) : _fs(fs),
                                                                                       _temperatureService(temperatureService),
                                                                                       _pump(pump)
{
}

MashService::~MashService() {}

DynamicJsonDocument jsonDocumentMash = DynamicJsonDocument(MAX_ACTIVESTATUS_SIZE);
JsonObject _mashSettings;

void MashService::LoadMashSettings()
{
    File configFile = _fs->open(MASH_SETTINGS_FILE, "r");
    if (configFile &&
        configFile.size() <= MAX_ACTIVESTATUS_SIZE &&
        deserializeJson(jsonDocumentMash, configFile) == DeserializationError::Ok && jsonDocumentMash.is<JsonObject>())
        _mashSettings = jsonDocumentMash.as<JsonObject>();
    configFile.close();
}

void MashService::loop(ActiveStatus *activeStatus)
{
    if (!activeStatus->BrewStarted || activeStatus->ActiveStep != mash)
        return;

    time_t timeNow = now();
    JsonArray steps = _mashSettings["st"].as<JsonArray>();
    if (activeStatus->TargetTemperature == 0)
    {
        JsonObject step = steps[0];
        activeStatus->TargetTemperature = step["t"];
        _pump->AntiCavitation();
        if (!activeStatus->BrewStarted || activeStatus->ActiveStep != mash)
            return;
    }

    if (activeStatus->EndTime > 0 && timeNow > activeStatus->EndTime)
    {
        Serial.println("Step over, next step: ");
        unsigned int nextMashStep = activeStatus->ActiveMashStepIndex + 1;
        if (steps.size() > nextMashStep)
        {
            JsonObject step = steps[nextMashStep];
            activeStatus->ActiveMashStepIndex = nextMashStep;
            activeStatus->StartTime = 0;
            activeStatus->EndTime = 0;
            activeStatus->TargetTemperature = step["t"];
            activeStatus->Recirculation = ((int)step["r"]) == 1;
            Buzzer().Ring(1, 2000);
            activeStatus->SaveActiveStatus();
        }
        else
        {
            Serial.println("Boil Time");
            activeStatus->ActiveStep = boil;
            activeStatus->StartTime = 0;
            activeStatus->EndTime = 0;
            activeStatus->ActiveMashStepIndex = -1;
            activeStatus->TargetTemperature = activeStatus->BoilTargetTemperature;
            activeStatus->Recirculation = false;
            activeStatus->Recirculation = false;
            Buzzer().Ring(2, 2000);
            _pump->TurnPumpOff();
            activeStatus->SaveActiveStatus();
        }
    }
    else
    {
        Serial.print("Temperature: ");
        Serial.println(activeStatus->Temperature);
        Serial.print("Target: ");
        Serial.println(activeStatus->TargetTemperature);

        if (activeStatus->StartTime == 0)
            // Recirculation while brew not started
            _pump->TurnPumpOn();
        else
            _pump->CheckRest();

        if (activeStatus->Temperature >= (activeStatus->TargetTemperature) && activeStatus->StartTime == 0)
        {
            Serial.println("Step Started");
            JsonObject step = steps[activeStatus->ActiveMashStepIndex];
            activeStatus->StartTime = timeNow;
            activeStatus->EndTime = timeNow + (int(step["tm"]) * 60);

            Serial.print("Start time: ");
            Serial.println(activeStatus->StartTime);
            Serial.print("End Time: ");
            Serial.println(activeStatus->EndTime);
            Buzzer().Ring(1, 2000);
            activeStatus->Recirculation = ((int)step["r"]) == 1;
            if (activeStatus->Recirculation)
                _pump->TurnPumpOn();
            else
                _pump->TurnPumpOff();
            activeStatus->SaveActiveStatus();
        }
    }
}