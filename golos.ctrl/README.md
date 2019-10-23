<img width="400" src="../docs/logo.jpg" />  

***  
[![GitHub](https://img.shields.io/github/license/cyberway/cyberway.contracts.svg)](https://github.com/cyberway/cyberway.contracts/blob/master/LICENSE)

# golos.ctrl

The `golos.ctrl` smart contract implements logic for election of witnesses, including the following:
  * a registering procedure for an account as a candidate for a witness;
  * a voting procedure for election of a witness;
  * determining a list of the most rated witnesses.  

This contract contains settings that apply to the Golos application as a whole. These settings can be used to the change parameters of any subsystems (for example, emission distribution between pools). Other Golos application smart contracts can access the `golos.ctrl` smart contract and get these settings, percentage ratios of the funds distributed by pools, limits on battery resources. Also, the smart contracts can obtain a list of the most rated witnesses with corresponding authority values. This makes it possible to verify the authenticity of actions certified by witnesses.  

More information about this contract can be found [here](https://cyberway.gitbook.io/en/devportal/application_contracts/golos_contracts/golos.ctrl_contract).


