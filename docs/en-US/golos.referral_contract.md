# The Referral Smart Contract

## Purpose of the golos.referral smart contract
The `golos.referral` smart contract contains the logic of a referral program that rewards users who invite their friends or third parties to register in the `Golos` application via social networks (for example, by viewing third-party publications or posting their own posts about the blockchain). The logic of the referral program defines one user as a referrer with respect to another (referral). The smart contract contains algorithm for calculating the reward for a post, as well as algorithm for the completion of the referral program, including the completion at the initiative of the referral user through the redemption of his account.

## Parameters set in the golos.referral smart contract 
The parameters of the smart contract are set by the witnesses (leaders) of the community.
```cpp
referral_param, types:[
    struct breakout_parametrs {
        asset min_breakout,
        asset max_breakout
    },
    expire_parametrs (uint64_t max_expire),
    percent_parametrs (uint32_t max_percent),
    delay_parametrs (uint32_t delay_clear_old_ref)
]
```

**Parameters:**  
  * `breakout_parametrs` — value in the structure form (containing fields):
    * `min_breakout` — the minimum allowable number of tokens required for the purchase of a referral account and, accordingly, the termination of the referral program;
    * `max_breakout` — the maximum allowable number of tokens required for the redemption of the referral account and, accordingly, the termination of the referral program.
  * `expire_parametrs `— the maximum allowable time of the referral program.
  * `percent_parametrs` — maximum allowable percentage of deduction to the referrer during the duration of the referral program.
  * `delay_parametrs` — period of time (mentioned in seconds) after which the action to delete obsolete entries from the table is launched.


## Actions used in golos.referral smart contract

The `golos.referral` smart contract supports the following actions: [setparams](#setparams), [validateprms](#validateprms), [addreferral](#addreferral) and [closeoldref](#closeoldref).

## setparams
The `setparams` action is used to set (configure) the parameters of a smart contract. The action has the form:
```cpp
void referral::setparams(std::vector<referral_params> params)
``` 
 The parameter `params`is a value in the form of a structure which contains the fields: `breakout_parametrs`, `expire_parametrs`, `percent_parametrs`, `delay_parametrs`.  


## validateprms
The `validateprms` action is called by the smart contract and is used to check the parameters for validity and controls the presence of errors in them.
```cpp
void referral::validateprms(std::vector<referral_params> params)
```
## addreferral
The `addreferral` action is used to create a referral account for the invited user. As a referrer, it can be specified a user who directly invited another user, as well as a third-party account. Referrer for the created referral account receives a share in the form of a percentage of the author's fees for referral publications.  

This action comes in the following the form:
```cpp
void referral::addreferral(
    name referrer,
    name referral,
    uint32_t percent,
    uint64_t expire,
    asset breakout
)
```
**Parameters:**  
  * `referral` — referral account name.  
  * `percent` — the percentage of payment to the referrer withdrawn from the referral income. The parameter takes a value from zero to the maximum allowed by the witnesses.  
  * `expire` — time (in seconds) of the referral program. The value must not exceed the maximum allowed time set by witnesses.  
  * `breakout` — the number of tokens which is necessary to purchase a referral account.  

To perform the `addreferral` action, the smart contract account authorization is required. To create a referral program record on a website, the witnesses (leaders) grant rights to this website, which becomes responsible for attracting users.
## closeoldref
The `closeoldref` action is a service internal function and is used to release obsolete entries from the table of active referral programs of the smart contract. It serves for removing referral programs data which actions are completed.  

The action has the following form:
```cpp
void referral::closeoldref()
```

To perform the `closeoldref` action, authorization is not required. The call is made automatically on addreferral() and paying breakout.

