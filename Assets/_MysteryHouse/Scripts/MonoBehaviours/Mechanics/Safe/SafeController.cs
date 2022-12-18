using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class SafeController : MechanismController
{
    [SerializeField]
    private GameObject safeDoorsBefore = null;
    [SerializeField]
    private GameObject safeDoorsAfter = null;
    [SerializeField]
    private SafeLeverController safeLeverController = null;
    [SerializeField]
    private SafeTurnLeverController safeTurnLeverController1 = null;
    [SerializeField]
    private SafeTurnLeverController safeTurnLeverController2 = null;
    [SerializeField]
    private SafeTurnLeverController safeTurnLeverController3 = null;

    [SerializeField]
    private AudioClip successSound = null;

    [SerializeField]
    private int firstCodeValue = 0;

    [SerializeField]
    private int secondCodeValue = 0;

    [SerializeField]
    private int thirdCodeValue = 0;

    private bool firstCodeFound = false;
    private bool secondCodeFound = false;
    private bool thirdCodeFound = false;
    
    private bool successCodeLock = false;
    private bool successLeverDown = false;

    private AudioSource audioSource = null;

    private void Awake()
    {
        audioSource = gameObject.AddComponent<AudioSource>();
    }

    void Start()
    {
        SetSafeDoorsAfter(false);
    }

    private void OnEnable()
    {
        safeLeverController.onSafeLeverDown += SafeLeverDown;
    }

    private void SetSafeDoorsAfter(bool isEnabledParam)
    {
        safeDoorsBefore.SetActive(!isEnabledParam);
        safeDoorsAfter.SetActive(isEnabledParam);
    }

    private void Update()
    {
        
        CheckCode();
    }

    private void CheckCode()
    {
        firstCodeFound = safeTurnLeverController1.CodeFound;
        secondCodeFound = safeTurnLeverController2.CodeFound;
        thirdCodeFound = safeTurnLeverController3.CodeFound;

        if (!successCodeLock) {
            if (firstCodeFound && secondCodeFound && thirdCodeFound)
            {
                successCodeLock = true;
                safeLeverController.SetSafeLeverEnabled();
            }
        }
    }

    private void PlayAudio(AudioClip clipParam)
    {
        if (!audioSource) return;

        audioSource.clip = clipParam;
        audioSource.Play();

    }

    private void SafeLeverDown()
    {
        successLeverDown = true;
        
    }

    public void UnityEvent_ReplaceDoors()
    {
        if (successLeverDown) {
            PlayAudio(successSound);
            SetSafeDoorsAfter(true);
        }
    }

    private void OnDisable()
    {
        safeLeverController.onSafeLeverDown -= SafeLeverDown;
    }
}
