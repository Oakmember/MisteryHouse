using Shared;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class ChangeHealthAreaController : MonoBehaviour
{

    [SerializeField]
    private bool isDamageArea = true;

    [SerializeField]
    private float duration = 1;

    [SerializeField]
    private int amount = 1;

    private bool isActivated = true;

    void Start()
    {

    }

    private void OnTriggerEnter(Collider other)
    {
        if (other.CompareTag(Consts.playerTag))
        {
            isActivated = true;
            if (isDamageArea)
            {
                DamageOverTime(amount, duration);
            }
            else
            {
                HealOverTime(amount, duration);
            }
        }
    }

    private void HealOverTime(int amountParam, float duration)
    {
        StartCoroutine(HealOverTimeCoroutine(amount, duration));
    }

    IEnumerator HealOverTimeCoroutine(int amountParam, float duration)
    {
        while (isActivated)
        {
            PlayerManager.Instance.SetPlayerHeal(amountParam);

            yield return new WaitForSeconds(duration);
        }
    }

    private void DamageOverTime(int amountParam, float duration)
    {
        StartCoroutine(DamageOverTimeCoroutine(amountParam, duration));
    }

    IEnumerator DamageOverTimeCoroutine(int amountParam, float duration)
    {

        while (isActivated)
        {
            PlayerManager.Instance.SetPlayerDamage(amountParam);

            yield return new WaitForSeconds(duration);
        }
    }

    private void OnTriggerExit(Collider other)
    {
        if (other.CompareTag(Consts.playerTag))
        {
            isActivated = false;
        }
    }
}
