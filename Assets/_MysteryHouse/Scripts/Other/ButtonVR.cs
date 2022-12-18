using HurricaneVR.Framework.Components;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Events;
using TMPro;
using System;

public class ButtonVR : MonoBehaviour
{
    [SerializeField]
    private TextMeshPro text = null;
    [SerializeField]
    private GameObject button = null;
    [SerializeField]
    private AudioClip pressSound = null;
    [SerializeField]
    private AudioClip releaseSound = null;
    [SerializeField]
    private UnityEvent onPress;
    [SerializeField]
    private UnityEvent onRelease;
    private GameObject presser = null;
    private AudioSource audioSource = null;
    private bool isPressed = false;
    [SerializeField]
    private char key;

    public TextMeshPro Text => text;
    public char Key
    { 
        get => key;
        set => key = value;
    }

    public ButtonVREvent ButtonDown = new ButtonVREvent();

    void Start()
    {
        audioSource = GetComponent<AudioSource>();
        isPressed = false;
    }

    private void OnTriggerEnter(Collider other)
    {
        if (!isPressed)
        {
            button.transform.localPosition = new Vector3(-0.003f, 0, 0);
            PlaySound(pressSound);
            presser = other.gameObject;
            onPress.Invoke();
            ButtonDown.Invoke(this);
            isPressed = true;
        }
    }

    private void OnTriggerExit(Collider other)
    {
        if (other.gameObject == presser)
        {
            button.transform.localPosition = new Vector3(0, 0, 0);
            //PlaySound(releaseSound);
            onRelease.Invoke();
            isPressed = false;
        }
    }

    private void PlaySound(AudioClip clipParam)
    {
        audioSource.clip = clipParam;
        audioSource.Play();
    }

}

[Serializable]
public class ButtonVREvent : UnityEvent<ButtonVR> { }