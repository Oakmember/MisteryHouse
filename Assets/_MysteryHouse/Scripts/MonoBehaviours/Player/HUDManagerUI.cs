using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using TMPro;

public class HUDManagerUI : MonoBehaviour
{
    [SerializeField] private HUDManager hudManager = null;
    [SerializeField] private Slider healthSlider = null;
    [SerializeField] private TextMeshProUGUI healthText = null;

    private void OnEnable()
    {
        hudManager.onSetHealthBar += SetHealthBar;
        hudManager.onInitializeHealthBar += SetInitializeHealthBar;
    }

    void Start()
    {
        
    }

    private void SetInitializeHealthBar(int healthParam)
    {
        healthSlider.value = healthParam;
        healthText.text = healthParam.ToString();
    }

    private void SetHealthBar(int healthParam)
    {
        healthSlider.value = healthParam;
        healthText.text = healthParam.ToString();
    }

    private void OnDisable()
    {
        hudManager.onSetHealthBar -= SetHealthBar;
        hudManager.onInitializeHealthBar -= SetInitializeHealthBar;
    }

}
