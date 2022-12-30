// ----------------------------------------------------------------------------------
// Initilizes BQ24195L_PMIC to allow battery operation and recharge
// ----------------------------------------------------------------------------------

bool init_PMIC()
{
  bool error = false;
  
  if (!PMIC.begin()) {
    Serial.println("ERROR: Failed to initialize PMIC!");
    return false;
  }

  // Set the input current limit to 1 A and the overload input voltage to 3.88 V
  if (!PMIC.setInputCurrentLimit(2.0)) { 
    Serial.println("ERROR: PMIC.setInputCurrentLimit() failed!");
    error = true;
  }

  if (!PMIC.setInputVoltageLimit(3.88)) {
    Serial.println("ERROR: PMIC.setInputVoltageLimit() failed!");
    error = true;
  }

  // set the minimum voltage used to feeding the module embed on Board
  if (!PMIC.setMinimumSystemVoltage(3.5)) {
    Serial.println("ERROR: PMIC.setMinimumSystemVoltage() failed!");
    error = true;
  }

  // Set the desired charge voltage to 4.2 V
  if (!PMIC.setChargeVoltage(4.2)) {
    Serial.println("ERROR: PMIC.setChargeVoltage() failed!");
    error = true;
  }

  // Set the charge current to 375 mA
  // the charge current should be defined as maximum at (C for hour)/2h
  // to avoid battery explosion (for example for a 750 mAh battery set to 0.375 A)
  if (!PMIC.setChargeCurrent(0.375)) {
    Serial.println("ERROR: PMIC.setChargeCurrent() failed!");
    error = true;
  }

  if (!PMIC.enableCharge()) {
    Serial.println("ERROR: PMIC.enableCharge() failed!");
    error = true;
  }
  delay(1000);
  
   int chargeStatus = PMIC.chargeStatus();
  switch(chargeStatus) {
    case NOT_CHARGING:
      Serial.println("Charge status: Not charging");
      break;

    case PRE_CHARGING:
      Serial.println("Charge status: Pre charging");
      break;

    case FAST_CHARGING:
      Serial.println("Charge status: Fast charging");
      break;

    case CHARGE_TERMINATION_DONE:
      Serial.println("Charge status: Charge termination done");
  }

  Serial.print("Battery is connected: ");
  if (PMIC.isBattConnected()) Serial.println("Yes");
  else Serial.println("No");

  Serial.print("Power is good: ");
  if (PMIC.isPowerGood()) Serial.println("Yes");
  else Serial.println("No");

  Serial.print("Charge current (A): ");
  Serial.println(PMIC.getChargeCurrent(),2);

  Serial.print("Charge voltage (V): ");
  Serial.println(PMIC.getChargeVoltage(), 2);

  Serial.print("Minimum system voltage(V): ");
  Serial.println(PMIC.getMinimumSystemVoltage(),2);

  Serial.print("Battery voltage is below minimum system voltage: ");
  if (!PMIC.canRunOnBattery()) Serial.println("No");
  else Serial.println("Yes");
  
  if (error) return false;
  else return true;
}
