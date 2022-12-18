using _BioMinds.Scripts.Data;
using Shared;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class SafeTurnLeverController : MonoBehaviour
{

    [SerializeField]
    private HingeJoint hingeJointRef = null;

    [SerializeField]
    private HingeJoint rigidbodyRef = null;

    [SerializeField]
    private int currentCodeValue = 0;

    [SerializeReference]
    private SafeController safeController = null;

    [SerializeField]
    private SafeTurnLeverSettings safeTurnLeverSettings = null;

    private bool codeLock = false;
    private bool codeFound = false;
    private int codeValue = 0;

    private AudioSource audioSource = null;

    public bool CodeFound => codeFound;

    private void Awake()
    {
        audioSource = gameObject.AddComponent<AudioSource>();
    }

    void Start()
    {
        SetCode();
    }

    private void SetCode()
    {
        int index1 = Random.Range(0, safeTurnLeverSettings.CodeValues.Count);
        currentCodeValue = safeTurnLeverSettings.CodeValues[index1];
    }

    // Update is called once per frame
    void Update()
    {
        if (codeLock) return;

        Turn();
        CheckCode();
    }

    private void Turn()
    {
        float turnValue = Mathf.Abs(hingeJointRef.angle);
        DebugManager.Instance.SetDebugText1(turnValue.ToString());
        if (turnValue >= 0 && turnValue < 20)
        {
            codeValue = 0;
        }
        else if (turnValue >= 20 && turnValue < 40)
        {
            codeValue = 10;
        }
        else if (turnValue >= 40 && turnValue < 60)
        {
            codeValue = 20;
        }
        else if (turnValue >= 60 && turnValue < 80)
        {
            codeValue = 30;
        }
        else if (turnValue >= 80 && turnValue < 100)
        {
            codeValue = 40;
        }
        else if (turnValue >= 100 && turnValue < 120)
        {
            codeValue = 50;
        }
        else if (turnValue >= 120 && turnValue < 140)
        {
            codeValue = 60;
        }
        else if (turnValue >= 140 && turnValue < 160)
        {
            codeValue = 70;
        }
        else if (turnValue >= 160 && turnValue < 180)
        {
            codeValue = 80;
        }

        DebugManager.Instance.SetDebugText2(codeValue.ToString());
    }

    private void CheckCode()
    {
        if (currentCodeValue == codeValue)
        {
            codeFound = true;
            codeLock = true;
            PlayAudio(safeTurnLeverSettings.FoundCodeSound);
            gameObject.layer = LayerMask.NameToLayer(Consts.nonInteractableLayer);
            //GameObject.Destroy(rigidbodyRef);
            //GameObject.Destroy(hingeJointRef);
        }
    }

    private void PlayAudio(AudioClip clipParam)
    {
        if (!audioSource) return;

        audioSource.clip = clipParam;
        audioSource.Play();

    }

    private void CodeValueFound(SafeTurnLeverController safeTurnLeverControllerParam)
    {

    }
}
